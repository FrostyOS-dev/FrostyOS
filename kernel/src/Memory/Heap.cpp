/*
Copyright (©) 2025-2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Heap.hpp"
#include "PagingUtil.hpp"
#include "PMM.hpp"
#include "VMM.hpp"

#include <assert.h>
#include <spinlock.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <util.h>

#include <HAL/HAL.hpp>

#define KMEM_MIN_LARGE_BUFS 8
#define KMEM_MAX_LARGE_SLAB (128 * 1024)

static_assert(sizeof(KMemSlab) < KMEM_SMALL_LIMIT);
static_assert(sizeof(KMemBufCtl) < KMEM_SMALL_LIMIT);

static inline bool IsPow2(size_t v) { return v != 0 && (v & (v - 1)) == 0; }
static inline size_t Log2(size_t v) { return 63 - static_cast<size_t>(__builtin_clzll(v)); }

static inline void*& Link(const KMemCache* cache, void* buf) {
    return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(buf) + cache->linkOffset);
}

static inline size_t HashIndex(const KMemCache* cache, void* buf) {
    uint64_t h = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buf)) * UINT64_C(0x9E3779B97F4A7C15);
    return static_cast<size_t>(h >> 32) & (cache->hashSize - 1);
}

static constexpr uint64_t g_ClassSizes[KMEM_NUM_GENERIC] = {
    16, 32, 48, 64, 80, 96, 112, 128,
    160, 192, 224, 256,
    320, 384, 448, 512,
    640, 768, 896, 1024,
    1280, 1536, 1792, 2048,
    2560, 3072, 4096, 5120, 6144, 8192
};

struct KMemClassMap {
    uint8_t idx[KMEM_MAX_GENERIC / 16 + 1];
    constexpr KMemClassMap() : idx() {
        int c = 0;
        for (size_t i = 0; i <= KMEM_MAX_GENERIC / 16; i++) {
            while (g_ClassSizes[c] < i * 16)
                c++;
            idx[i] = static_cast<uint8_t>(c);
        }
    }
};

static constexpr KMemClassMap g_ClassMap;

static inline int ClassIndex(size_t size) {
    if (__builtin_expect(size > KMEM_MAX_GENERIC, 0))
        return -1;
    return g_ClassMap.idx[(size + 15) >> 4];
}

void KMemSlabList::PushFront(KMemSlab* s) {
    s->prev = nullptr;
    s->next = head;
    if (head != nullptr)
        head->prev = s;
    else
        tail = s;
    head = s;
    count++;
}

void KMemSlabList::PushBack(KMemSlab* s) {
    s->next = nullptr;
    s->prev = tail;
    if (tail != nullptr)
        tail->next = s;
    else
        head = s;
    tail = s;
    count++;
}

void KMemSlabList::Remove(KMemSlab* s) {
    if (s->prev != nullptr)
        s->prev->next = s->next;
    else
        head = s->next;
    if (s->next != nullptr)
        s->next->prev = s->prev;
    else
        tail = s->prev;
    s->next = nullptr;
    s->prev = nullptr;
    count--;
}

KMemSlab* KMemSlabList::PopBack() {
    KMemSlab* s = tail;
    if (s != nullptr)
        Remove(s);
    return s;
}

void SlabHeap::Init(KMemBackend* backend) {
    m_backend = backend;

    CacheInit(&m_slabCache, "kmem_slab_cache", sizeof(KMemSlab), 8, nullptr, nullptr, nullptr, 0);
    CacheInit(&m_bufCtlCache, "kmem_bufctl_cache", sizeof(KMemBufCtl), 8, nullptr, nullptr, nullptr, 0);
    assert(!m_slabCache.isLarge && !m_bufCtlCache.isLarge);
    CacheInit(&m_cacheCache, "kmem_cache_cache", sizeof(KMemCache), 64, nullptr, nullptr, nullptr, 0);

    LinkCache(&m_slabCache);
    LinkCache(&m_bufCtlCache);
    LinkCache(&m_cacheCache);

    memset(m_generic, 0, sizeof(m_generic));
    memset(m_aligned, 0, sizeof(m_aligned));
    for (int i = 0; i < KMEM_NUM_GENERIC; i++) {
        m_generic[i] = CacheCreate("kmem_alloc", g_ClassSizes[i], 16, nullptr, nullptr, nullptr, 0);
        if (m_generic[i] == nullptr)
            PANIC("Failed to create generic kmem cache");
    }
}

KMemCache* SlabHeap::CacheCreate(const char* name, size_t size, size_t align, KMemCtor ctor, KMemDtor dtor, void* priv, int flags) {
    KMemCache* cache = static_cast<KMemCache*>(CacheAlloc(&m_cacheCache, KM_SLEEP));
    if (cache == nullptr)
        return nullptr;
    CacheInit(cache, name, size, align, ctor, dtor, priv, flags);
    LinkCache(cache);
    return cache;
}

void SlabHeap::CacheDestroy(KMemCache* cp) {
    m_cacheListLock.Lock();

    KMemCache** pp = &m_cacheList;
    while (*pp != nullptr && *pp != cp)
        pp = &(*pp)->nextCache;
    assert(*pp == cp);
    *pp = cp->nextCache;

    spinlock_acquire_int(&cp->lock);
    assert(cp->full.count == 0 && cp->partial.count <= 1);
    if (cp->partial.count == 1) {
        KMemSlab* last = cp->partial.head;
        assert(last->refCount == 0);
        cp->partial.Remove(last);
        cp->empty.PushBack(last);
    }
    KMemSlab* list = nullptr;
    while (KMemSlab* s = cp->empty.PopBack()) {
        s->next = list;
        list = s;
    }
    spinlock_release_int(&cp->lock);
    m_cacheListLock.Unlock();

    while (list != nullptr) {
        KMemSlab* next = list->next;
        SlabDestroy(cp, list);
        list = next;
    }

    if (cp->isLarge && cp->hash != cp->hashInitial)
        m_backend->Free(cp->hash, ALIGN_UP_BASE2(cp->hashSize * sizeof(KMemBufCtl*), PAGE_SIZE));

    CacheFree(&m_cacheCache, cp);
}

void* SlabHeap::CacheAlloc(KMemCache* cache, int kmflag) {
    while (true) {
        spinlock_acquire_int(&cache->lock);
        KMemSlab* slab = cache->partial.head;
        bool fromEmpty = false;
        if (slab == nullptr) {
            slab = cache->empty.head;
            fromEmpty = true;
        }
        if (slab != nullptr) {
            void* buf = TakeBuf(cache, slab, fromEmpty);
            cache->allocs++;
            spinlock_release_int(&cache->lock);
            return buf;
        }
        spinlock_release_int(&cache->lock);

        if (!CacheGrow(cache, kmflag))
            return nullptr;
    }
}

void SlabHeap::CacheFree(KMemCache* cp, void* buf) {
    spinlock_acquire_int(&cp->lock);

    KMemSlab* slab;
    if (!cp->isLarge) {
        slab = SmallSlabFromBuf(buf);
        if (slab->cache != cp || slab->refCount == 0)
            PANIC("Heap: bad free (wrong cache or size, or double free)");
        Link(cp, buf) = slab->freelist.buf;
        slab->freelist.buf = buf;
    } else {
        KMemBufCtl* bc = HashRemove(cp, buf);
        if (bc == nullptr)
            PANIC("Heap: bad free (unknown address, wrong size, or double free)");
        slab = bc->slab;
        bc->next = slab->freelist.bufctl;
        slab->freelist.bufctl = bc;
    }

    bool wasFull = (slab->refCount == cp->bufsPerSlab);
    slab->refCount--;

    if (wasFull) {
        cp->full.Remove(slab);
        if (slab->refCount == 0 && cp->partial.count != 0)
            cp->empty.PushBack(slab);
        else
            cp->partial.PushFront(slab);
    } else if (slab->refCount == 0 && cp->partial.count > 1) {
        cp->partial.Remove(slab);
        cp->empty.PushBack(slab);
    }
    cp->frees++;
    spinlock_release_int(&cp->lock);
}

void* SlabHeap::Alloc(size_t size, int kmflag) {
    int idx = ClassIndex(size);
    if (idx >= 0)
        return CacheAlloc(m_generic[idx], kmflag);
    return AllocPages(size);
}

void SlabHeap::Free(void* ptr, size_t size) {
    if (ptr == nullptr)
        return;
    int idx = ClassIndex(size);
    if (idx >= 0)
        CacheFree(m_generic[idx], ptr);
    else
        FreePages(ptr, size);
}

void* SlabHeap::AllocAligned(size_t size, size_t align, int kmflag) {
    if (!IsPow2(align))
        return nullptr;
    if (align <= 16)
        return Alloc(size, kmflag);
    if (align > PAGE_SIZE)
        return nullptr;

    int idx = ClassIndex(size);
    if (idx < 0)
        return AllocPages(size);

    KMemCache* cp = AlignedCache(idx, align, true);
    return cp != nullptr ? CacheAlloc(cp, kmflag) : nullptr;
}

void SlabHeap::FreeAligned(void* ptr, size_t size, size_t align) {
    if (ptr == nullptr)
        return;
    if (align <= 16)
        return Free(ptr, size);
    int idx = ClassIndex(size);
    if (idx < 0)
        return FreePages(ptr, size);
    KMemCache* cp = AlignedCache(idx, align, false);
    if (cp == nullptr)
        PANIC("Heap: aligned free with no matching cache");
    CacheFree(cp, ptr);
}

void* SlabHeap::AllocTagged(size_t size, int kmflag) {
    if (size > SIZE_MAX - KMEM_TAG_SIZE - PAGE_SIZE)
        return nullptr;
    size_t total = size + KMEM_TAG_SIZE;

    KMemTag* tag;
    int idx = ClassIndex(total);
    if (idx >= 0) {
        KMemCache* cp = m_generic[idx];
        tag = static_cast<KMemTag*>(CacheAlloc(cp, kmflag));
        if (tag == nullptr)
            return nullptr;
        tag->cache = cp;
        tag->size = 0;
    } else {
        size_t bytes = ALIGN_UP_BASE2(total, PAGE_SIZE);
        tag = static_cast<KMemTag*>(m_backend->Allocate(bytes));
        if (tag == nullptr)
            return nullptr;
        tag->cache = nullptr;
        tag->size = bytes - KMEM_TAG_SIZE;
    }
    return reinterpret_cast<uint8_t*>(tag) + KMEM_TAG_SIZE;
}

void SlabHeap::FreeTagged(void* ptr) {
    if (ptr == nullptr)
        return;
    KMemTag* tag = reinterpret_cast<KMemTag*>(static_cast<uint8_t*>(ptr) - KMEM_TAG_SIZE);
    if (tag->cache != nullptr)
        CacheFree(tag->cache, tag);
    else
        m_backend->Free(tag, tag->size + KMEM_TAG_SIZE);
}

size_t SlabHeap::TaggedSize(void* ptr) const {
    if (ptr == nullptr)
        return 0;
    const KMemTag* tag = reinterpret_cast<KMemTag*>(static_cast<uint8_t*>(ptr) - KMEM_TAG_SIZE);
    return tag->cache != nullptr ? tag->cache->objSize - KMEM_TAG_SIZE : tag->size;
}

void SlabHeap::Reap() {
    m_cacheListLock.Lock();
    for (KMemCache* cp = m_cacheList; cp != nullptr; cp = cp->nextCache)
        ReapCache(cp);
    m_cacheListLock.Unlock();
}

void SlabHeap::CacheInit(KMemCache* cache, const char* name, size_t size, size_t align, KMemCtor ctor, KMemDtor dtor, void* priv, int flags) {
    memset(cache, 0, sizeof(KMemCache));
    if (align < KMEM_MIN_ALIGN)
        align = KMEM_MIN_ALIGN;
    assert(IsPow2(align) && align <= PAGE_SIZE);
    if (size < sizeof(void*))
        size = sizeof(void*);

    cache->name = name;
    cache->objSize = size;
    cache->align = align;
    cache->ctor = ctor;
    cache->dtor = dtor;
    cache->priv = priv;
    cache->flags = flags;
    cache->heap = this;
    spinlock_init(&cache->lock);

    ComputeLayout(cache);

    cache->hash = cache->hashInitial;
    cache->hashSize = KMEM_HASH_INIT_SIZE;
}

void SlabHeap::ComputeLayout(KMemCache* cache) {
    const size_t a = cache->align;

    size_t extra = (cache->ctor != nullptr || cache->dtor != nullptr) ? sizeof(void*) : 0;
    size_t smallBuf = ALIGN_UP_BASE2(cache->objSize + extra, a);

    if (smallBuf < KMEM_SMALL_LIMIT) {
        size_t usable = PAGE_SIZE - sizeof(KMemSlab);
        cache->isLarge = false;
        cache->bufSize = smallBuf;
        cache->linkOffset = smallBuf - sizeof(void*);
        cache->slabSize = PAGE_SIZE;
        cache->bufsPerSlab = usable / smallBuf;
        cache->colourMax = ALIGN_DOWN_BASE2(usable - cache->bufsPerSlab * smallBuf, a);
        return;
    }

    cache->isLarge = true;
    cache->bufSize = ALIGN_UP_BASE2(cache->objSize, a);
    cache->linkOffset = 0;
    for (size_t pages = 1; ; pages++) {
        size_t size = pages * PAGE_SIZE;
        size_t n = size / cache->bufSize;
        if (n == 0)
            continue;
        size_t waste = size - n * cache->bufSize;
        if (((n >= KMEM_MIN_LARGE_BUFS || size >= KMEM_MAX_LARGE_SLAB) && waste * 8 <= size)) {
            cache->slabSize = size;
            cache->bufsPerSlab = n;
            cache->colourMax = ALIGN_DOWN_BASE2(waste, a);
            return;
        }
    }
}

void SlabHeap::LinkCache(KMemCache* cache) {
    m_cacheListLock.Lock();
    cache->nextCache = m_cacheList;
    m_cacheList = cache;
    m_cacheListLock.Unlock();
}

bool SlabHeap::CacheGrow(KMemCache* cp, int kmflag) {
    spinlock_acquire_int(&cp->lock);
    size_t colour = cp->colorNext;
    cp->colorNext = (colour + cp->align > cp->colourMax) ? 0 : colour + cp->align;
    size_t slabs = cp->full.count + cp->partial.count + cp->empty.count;
    spinlock_release_int(&cp->lock);

    if (cp->isLarge && !HashReserve(cp, (slabs + 1) * cp->bufsPerSlab))
        return false;

    uint8_t* base = static_cast<uint8_t*>(m_backend->Allocate(cp->slabSize));
    if (base == nullptr)
        return false;
    assert((reinterpret_cast<uintptr_t>(base) & (PAGE_SIZE - 1)) == 0);

    KMemSlab* slab;
    if (!cp->isLarge)
        slab = reinterpret_cast<KMemSlab*>(base + PAGE_SIZE - sizeof(KMemSlab));
    else {
        slab = static_cast<KMemSlab*>(CacheAlloc(&m_slabCache, kmflag));
        if (slab == nullptr) {
            m_backend->Free(base, cp->slabSize);
            return false;
        }
    }
    slab->next = nullptr;
    slab->prev = nullptr;
    slab->base = base;
    slab->cache = cp;
    slab->refCount = 0;
    slab->colour = static_cast<uint32_t>(colour);
    slab->freelist.buf = nullptr;

    // Build the freelist in address order
    if (!cp->isLarge) {
        void* tailBuf = nullptr;
        size_t built = 0;
        bool ok = true;
        for (; built < cp->bufsPerSlab; built++) {
            uint8_t* buf = base + colour + built * cp->bufSize;
            if (cp->ctor != nullptr && cp->ctor(buf, cp->priv, kmflag) != 0) {
                ok = false;
                break;
            }
            Link(cp, buf) = nullptr;
            if (tailBuf != nullptr)
                Link(cp, tailBuf) = buf;
            else
                slab->freelist.buf = buf;
            tailBuf = buf;
        }
        if (!ok) {
            if (cp->dtor != nullptr) {
                for (size_t j = 0; j < built; j++)
                    cp->dtor(base + colour + j * cp->bufSize, cp->priv);
            }
            m_backend->Free(base, cp->slabSize);
            return false;
        }
    } else {
        KMemBufCtl* tailBc = nullptr;
        size_t built = 0;
        bool ok = true;
        for (; built < cp->bufsPerSlab; built++) {
            KMemBufCtl* bc = static_cast<KMemBufCtl*>(CacheAlloc(&m_bufCtlCache, kmflag));
            if (bc == nullptr) {
                ok = false;
                break;
            }
            bc->buf = base + colour + built * cp->bufSize;
            bc->slab = slab;
            bc->next = nullptr;
            if (cp->ctor != nullptr && cp->ctor(bc->buf, cp->priv, kmflag) != 0) {
                CacheFree(&m_bufCtlCache, bc);
                ok = false;
                break;
            }
            if (tailBc)
                tailBc->next = bc;
            else
                slab->freelist.bufctl = bc;
            tailBc = bc;
        }
        if (!ok) {
            KMemBufCtl* bc = slab->freelist.bufctl;
            while (bc != nullptr) {
                KMemBufCtl* next = bc->next;
                if (cp->dtor != nullptr)
                    cp->dtor(bc->buf, cp->priv);
                CacheFree(&m_bufCtlCache, bc);
                bc = next;
            }
            CacheFree(&m_slabCache, slab);
            m_backend->Free(base, cp->slabSize);
            return false;
        }
    }

    spinlock_acquire_int(&cp->lock);
    cp->empty.PushBack(slab);
    cp->slabsCreated++;
    spinlock_release_int(&cp->lock);
    return true;
}

void SlabHeap::SlabDestroy(KMemCache* cp, KMemSlab* slab) {
    assert(slab->refCount == 0);
    void* base = slab->base;

    if (!cp->isLarge) {
        if (cp->dtor != nullptr) {
            uint8_t* first = static_cast<uint8_t*>(base) + slab->colour;
            for (size_t i = 0; i < cp->bufsPerSlab; i++)
                cp->dtor(first + i * cp->bufSize, cp->priv);
        }
    } else {
        KMemBufCtl* bc = slab->freelist.bufctl;
        while (bc != nullptr) {
            KMemBufCtl* next = bc->next;
            if (cp->dtor != nullptr)
                cp->dtor(bc->buf, cp->priv);
            CacheFree(&m_bufCtlCache, bc);
            bc = next;
        }
        CacheFree(&m_slabCache, slab);
    }
    m_backend->Free(base, cp->slabSize);
}

void SlabHeap::ReapCache(KMemCache* cp) {
    KMemSlab* list = nullptr;

    spinlock_acquire_int(&cp->lock);
    size_t n = cp->emptyMin < cp->empty.count ? cp->emptyMin : cp->empty.count;
    while (n-- > 0) {
        KMemSlab* s = cp->empty.PopBack();
        s->next = list;
        list = s;
        cp->slabsDestroyed++;
    }
    cp->emptyMin = cp->empty.count;
    spinlock_release_int(&cp->lock);

    while (list != nullptr) {
        KMemSlab* next = list->next;
        SlabDestroy(cp, list);
        list = next;
    }
}

void* SlabHeap::TakeBuf(KMemCache* cache, KMemSlab* slab, bool fromEmpty) {
    void* buf;
    if (!cache->isLarge) {
        buf = slab->freelist.buf;
        slab->freelist.buf = Link(cache, buf);
    } else {
        KMemBufCtl* bc = slab->freelist.bufctl;
        slab->freelist.bufctl = bc->next;
        buf = bc->buf;
        HashInsert(cache, bc);
    }

    slab->refCount++;
    bool nowFull = (slab->refCount == cache->bufsPerSlab);

    if (fromEmpty) {
        cache->empty.Remove(slab);
        if (cache->empty.count < cache->emptyMin)
            cache->emptyMin = cache->empty.count;
        (nowFull ? cache->full : cache->partial).PushFront(slab);
    } else if (nowFull) {
        cache->partial.Remove(slab);
        cache->full.PushFront(slab);
    }
    return buf;
}

KMemSlab* SlabHeap::SmallSlabFromBuf(void* buf) const {
    uintptr_t page = reinterpret_cast<uintptr_t>(buf) & ~static_cast<uintptr_t>(PAGE_SIZE - 1);
    return reinterpret_cast<KMemSlab*>(page + PAGE_SIZE - sizeof(KMemSlab));
}

bool SlabHeap::HashReserve(KMemCache* cp, size_t neededBufs) {
    spinlock_acquire_int(&cp->lock);
    size_t cur = cp->hashSize;
    spinlock_release_int(&cp->lock);

    if (neededBufs <= cur * 2)
        return true;

    size_t newSize = cur;
    while (neededBufs > newSize * 2)
        newSize <<= 1;
    size_t newBytes = ALIGN_UP_BASE2(newSize * sizeof(KMemBufCtl*), PAGE_SIZE);

    KMemBufCtl** table = static_cast<KMemBufCtl**>(m_backend->Allocate(newBytes));
    if (table == nullptr)
        return false;
    memset(table, 0, newBytes);

    spinlock_acquire_int(&cp->lock);
    if (cp->hashSize >= newSize) { // already grown elsewhere
        spinlock_release_int(&cp->lock);
        m_backend->Free(table, newBytes);
        return true;
    }
    KMemBufCtl** old = cp->hash;
    size_t oldSize = cp->hashSize;
    cp->hash = table;
    cp->hashSize = newSize;
    for (size_t i = 0; i < oldSize; i++) {
        KMemBufCtl* bc = old[i];
        while (bc != nullptr) {
            KMemBufCtl* next = bc->next;
            size_t j = HashIndex(cp, bc->buf);
            bc->next = table[j];
            table[j] = bc;
            bc = next;
        }
    }
    spinlock_release_int(&cp->lock);

    if (old != cp->hashInitial)
        m_backend->Free(old, ALIGN_UP_BASE2(oldSize * sizeof(KMemBufCtl*), PAGE_SIZE));
    return true;
}

void SlabHeap::HashInsert(KMemCache* cp, KMemBufCtl* bc) {
    size_t i = HashIndex(cp, bc->buf);
    bc->next = cp->hash[i];
    cp->hash[i] = bc;
}

KMemBufCtl* SlabHeap::HashRemove(KMemCache* cp, void* buf) {
    KMemBufCtl** pp = &cp->hash[HashIndex(cp, buf)];
    while (*pp != nullptr) {
        if ((*pp)->buf == buf) {
            KMemBufCtl* bc = *pp;
            *pp = bc->next;
            return bc;
        }
        pp = &(*pp)->next;
    }
    return nullptr;
}

void* SlabHeap::AllocPages(size_t size) {
    if (size > SIZE_MAX - PAGE_SIZE)
        return nullptr;
    return m_backend->Allocate(ALIGN_UP_BASE2(size, PAGE_SIZE));
}

void SlabHeap::FreePages(void* ptr, size_t size) {
    return m_backend->Free(ptr, ALIGN_UP_BASE2(size, PAGE_SIZE));
}

KMemCache* SlabHeap::AlignedCache(int classIdx, size_t align, bool create) {
    KMemCache** slot = &m_aligned[classIdx][Log2(align) - 5]; // 32 -> 0
    KMemCache* cp = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (cp != nullptr || !create)
        return cp;

    size_t objSize = ALIGN_UP_BASE2(g_ClassSizes[classIdx], align);
    cp = CacheCreate("kmem_aligned", objSize, align, nullptr, nullptr, nullptr, 0);
    if (cp == nullptr)
        return nullptr;

    KMemCache* expected = nullptr;
    if (!__atomic_compare_exchange_n(slot, &expected, cp, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        CacheDestroy(cp);
        cp = expected;
    }
    return cp;
}

static KMemBackend g_VMMBackend = {
    [](size_t size) -> void* {
        void* p = g_PMM->AllocatePages(DIV_ROUNDUP(size, PAGE_SIZE));
        if (p == nullptr)
            return nullptr;
        return to_HHDM(p);
    },
    [](void* ptr, size_t size) {
        g_PMM->FreePages(from_HHDM(ptr), DIV_ROUNDUP(size, PAGE_SIZE));
    }
};

static KMemBackend g_KBackend = {
    [](size_t size) -> void* {
        return VMM::g_KVMM->AllocateAnonPages(DIV_ROUNDUP(size, PAGE_SIZE), {VMM::Protection::READ_WRITE, VMM::CacheType::DEFAULT, false, true, false, true, true, false});
    },
    [](void* ptr, size_t) {
        if (!VMM::g_KVMM->FreePages(ptr))
            PANIC("Kernel heap failed to free section");
    }
};

SlabHeap g_VMMHeap;
SlabHeap g_KHeap;

void InitVMMHeap() {
    g_VMMHeap.Init(&g_VMMBackend);
}

void InitKHeap() {
    g_KHeap.Init(&g_KBackend);
}

extern "C" {

void* kmalloc_sized(size_t size) {
    return g_KHeap.Alloc(size);
}

void kfree_sized(void* ptr, size_t size) {
    g_KHeap.Free(ptr, size);
}

void* kmalloc_aligned(size_t size, size_t al) {
    return g_KHeap.AllocAligned(size, al);
}

void kfree_aligned(void* p, size_t s, size_t a) {
    g_KHeap.FreeAligned(p, s, a);
}

void* kmalloc(size_t size) {
    return g_KHeap.AllocTagged(size);
}

void kfree(void* ptr) {
    g_KHeap.FreeTagged(ptr);
}

void* kcalloc(size_t num, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(num, size, &total))  
        return nullptr;
    void* ptr = g_KHeap.AllocTagged(total);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, total);
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    if (ptr == nullptr)
        return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return nullptr;
    }
    size_t oldSize = g_KHeap.TaggedSize(ptr);
    if (oldSize >= size)
        return ptr;
    void* newBuf = kmalloc(size);
    if (newBuf == nullptr)
        return nullptr;
    memcpy(newBuf, ptr, oldSize);
    kfree(ptr);
    return newBuf;
}

void* kmalloc_vmm(size_t size) {
    return g_VMMHeap.Alloc(size);
}

void kfree_vmm(void* ptr, size_t size) {
    g_VMMHeap.Free(ptr, size);
}

void* kcalloc_vmm(size_t num, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(num, size, &total))  
        return nullptr;
    void* ptr = g_VMMHeap.Alloc(total);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, total);
    return ptr;
}

}
