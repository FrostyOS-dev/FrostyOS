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

#ifndef _KERNEL_HEAP_HPP
#define _KERNEL_HEAP_HPP

#include <stddef.h>
#include <stdint.h>
#include <spinlock.h>
#include <util.h>

#include <Scheduling/Mutex.hpp>

#define KMEM_MIN_ALIGN 8
#define KMEM_SMALL_LIMIT (PAGE_SIZE / 8)
#define KMEM_TAG_SIZE 16
#define KMEM_NUM_GENERIC 30
#define KMEM_MAX_GENERIC 8192
#define KMEM_HASH_INIT_SIZE 16
#define KMEM_ALIGN_LEVELS 8

enum : int {
    KM_SLEEP = 0,
    KM_NOSLEEP = 1
};

struct KMemBackend {
    void* (*Allocate)(size_t size);
    void (*Free)(void* ptr, size_t size);
};

typedef int (*KMemCtor)(void* obj, void* priv, int kmFlag);
typedef void (*KMemDtor)(void* obj, void* priv);

struct KMemCache;
struct KMemSlab;
class  SlabHeap;

struct KMemBufCtl {
    KMemBufCtl* next;
    void* buf;
    KMemSlab* slab;
};

struct KMemSlab {
    KMemSlab* next;
    KMemSlab* prev;
    void* base;
    KMemCache* cache;
    union {
        void* buf;
        KMemBufCtl* bufctl;
    } freelist;
    uint32_t refCount;
    uint32_t colour;
};

struct KMemSlabList {
    KMemSlab* head;
    KMemSlab* tail;
    size_t count;
    void PushFront(KMemSlab* s);
    void PushBack(KMemSlab* s);
    void Remove(KMemSlab* s);
    KMemSlab* PopBack();
};

struct KMemCache {
    // client supplied
    const char* name;
    size_t objSize;
    size_t align;
    KMemCtor ctor;
    KMemDtor dtor;
    void* priv;
    int flags;

    // derived in ComputeLayout
    bool isLarge;
    size_t bufSize;
    size_t linkOffset;
    size_t slabSize;
    size_t bufsPerSlab;
    size_t colourMax;

    // mutable, protected by lock
    spinlock_t lock;
    size_t colorNext;
    KMemSlabList full;
    KMemSlabList partial;
    KMemSlabList empty;
    size_t emptyMin;
    KMemBufCtl** hash;
    size_t hashSize;
    KMemBufCtl* hashInitial[KMEM_HASH_INIT_SIZE];

    size_t allocs;
    size_t frees;
    size_t slabsCreated;
    size_t slabsDestroyed;

    KMemCache* nextCache;
    SlabHeap* heap;
};

struct KMemTag {
    KMemCache* cache;
    size_t size;
};
static_assert(sizeof(KMemTag) == KMEM_TAG_SIZE);

class SlabHeap {
public:
    void Init(KMemBackend* backend);

    KMemCache* CacheCreate(const char* name, size_t size, size_t align, KMemCtor ctor, KMemDtor dtor, void* priv, int flags);
    void CacheDestroy(KMemCache* cache);
    void* CacheAlloc(KMemCache* cache, int kmflag);
    void CacheFree(KMemCache* cache, void* obj);

    void* Alloc(size_t size, int kmflag = KM_SLEEP);
    void Free(void* ptr, size_t size);

    // aligned, power of 2 align <= PAGE_SIZE
    void* AllocAligned(size_t size, size_t align, int kmflag = KM_SLEEP);
    void FreeAligned(void* ptr, size_t size, size_t align);

    void* AllocTagged(size_t size, int kmflag = KM_SLEEP);
    void FreeTagged(void* ptr);
    size_t TaggedSize(void* ptr) const;

    // Releases slabs that have stayed empty since the last call
    void Reap();

private:
    void CacheInit(KMemCache* cache, const char* name, size_t size, size_t align, KMemCtor ctor, KMemDtor dtor, void* priv, int flags);
    void ComputeLayout(KMemCache* cache);
    void LinkCache(KMemCache* cache);
    bool CacheGrow(KMemCache* cache, int kmflag);
    void SlabDestroy(KMemCache* cp, KMemSlab* slab);
    void ReapCache(KMemCache* cache);

    void* TakeBuf(KMemCache* cache, KMemSlab* slab, bool fromEmpty);
    KMemSlab* SmallSlabFromBuf(void* buf) const;

    bool HashReserve(KMemCache* cache, size_t neededBufs); // lock NOT held
    void HashInsert(KMemCache* cache, KMemBufCtl* bc);
    KMemBufCtl* HashRemove(KMemCache* cache, void* buf);

    void* AllocPages(size_t size);
    void FreePages(void* ptr, size_t size);
    KMemCache* AlignedCache(int classIdx, size_t align, bool create);

    KMemBackend* m_backend = nullptr;

    KMemCache m_slabCache;
    KMemCache m_bufCtlCache;
    KMemCache m_cacheCache;

    KMemCache* m_generic[KMEM_NUM_GENERIC];
    KMemCache* m_aligned[KMEM_NUM_GENERIC][KMEM_ALIGN_LEVELS]; // lazily created

    KMemCache* m_cacheList = nullptr;
    Mutex m_cacheListLock; // create/destroy/reap only, never on alloc path
};

extern SlabHeap g_VMMHeap;
extern SlabHeap g_KHeap;

void InitVMMHeap();
void InitKHeap();

#endif /* _KERNEL_HEAP_HPP */