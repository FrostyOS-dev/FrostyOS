/*
Copyright (©) 2026  Frosty515

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

#include "PMM.hpp"
#include "PageMapper.hpp"
#include "Pager.hpp"
#include "PagingUtil.hpp"
#include "VMM.hpp"
#include "VMRegionAllocator.hpp"

#include <spinlock.h>
#include <stdint.h>
#include <stdlib.h>
#include <util.h>

#include <DataStructures/AVLTree.hpp>

namespace VMM {

    static inline Protection WithoutWrite(Protection p) {
        return static_cast<Protection>(static_cast<uint8_t>(p) & ~static_cast<uint8_t>(Protection::WRITE));
    }

    static inline bool isLessOrEqualProt(Protection a, Protection b) {
        uint64_t aProt = static_cast<uint64_t>(a);
        uint64_t bProt = static_cast<uint64_t>(b);
        constexpr uint64_t read = static_cast<uint64_t>(Protection::READ);
        constexpr uint64_t write = static_cast<uint64_t>(Protection::WRITE);
        constexpr uint64_t execute = static_cast<uint64_t>(Protection::EXECUTE);
        if (aProt == bProt)
            return true;
        if ((aProt & read) > (bProt & read))
            return false;
        if ((aProt & write) > (bProt & write))
            return false;
        if ((aProt & execute) > (bProt & execute))
            return false;
        return true;
    }

    static inline FrameDesc& FD(uint64_t phys) {
        return g_frameTable[phys >> PAGE_SIZE_SHIFT];
    }

    static inline void FrameRef(uint64_t phys) {
        __atomic_fetch_add(&FD(phys).refCount, 1, __ATOMIC_ACQ_REL);
    }

    static inline bool PageIsShared(Page* p) {
        return p->physAddr != 0 && __atomic_load_n(&FD(p->physAddr).refCount, __ATOMIC_ACQUIRE) > 1;
    }

    static inline bool FrameUnref(uint64_t phys) {
        return __atomic_sub_fetch(&FD(phys).refCount, 1, __ATOMIC_ACQ_REL) == 0;
    }

    void RefMemObj(MemoryObject* obj) {
        __atomic_fetch_add(&obj->refCount, 1, __ATOMIC_ACQ_REL);
    }

    bool UnrefMemObj(MemoryObject* obj) {
        if (__atomic_sub_fetch(&obj->refCount, 1, __ATOMIC_RELEASE) == 0) {
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            return true;
        }
        return false;
    }

    bool MemObjIsShared(MemoryObject* obj) {
        return __atomic_load_n(&obj->refCount, __ATOMIC_ACQUIRE) > 1;
    }

    static Page* NewAnonPage(const Page* copyFrom, bool zero) {
        Page* p = (Page*)kcalloc_vmm(1, sizeof(Page));
        if (p == nullptr)
            return nullptr;
        void* frame = g_PMM->AllocatePage();
        if (frame == nullptr) {
            kfree_vmm(p);
            return nullptr;
        }
        if (copyFrom != nullptr)
            memcpy(to_HHDM(frame), to_HHDM((void*)copyFrom->physAddr), PAGE_SIZE);
        else if (zero)
            memset(to_HHDM(frame), 0, PAGE_SIZE);
        p->physAddr = (uint64_t)frame;
        p->protection = Protection::READ_WRITE_EXECUTE;
        __atomic_store_n(&FD(p->physAddr).refCount, 1, __ATOMIC_RELEASE);
        return p;
    }

    static void ReleasePage(MemoryObject* o, Page* p) {
        if (p->physAddr != 0) {
            if (o->kind == ObjectKind::Anon) {
                if (FrameUnref(p->physAddr))
                    g_PMM->FreePage((void*)p->physAddr);
            } else if (o->pager != nullptr) // use the pager when available
                o->pager->FreePage((void*)p->physAddr);
            else
                g_PMM->FreePage((void*)p->physAddr);
        }
        kfree_vmm(p);
    }

    // lock is held, refCount == 0
    static void DestroyMemoryObject(MemoryObject* obj) {
        obj->pages.Drain([&](Page* page) -> void {
            ReleasePage(obj, page);
        });
        kfree_vmm(obj);
    }

    static bool EnsureResident(MemoryObject* o, Page* p, bool zero) {
        if (p->physAddr != 0)
            return true;
        void* f = o->pager->AllocatePage();
        if (f == nullptr)
            return false;
        if (zero)
            memset(to_HHDM(f), 0, PAGE_SIZE);
        p->physAddr = (uint64_t)f;
        return true;
    }

    void* MemoryObject::operator new(size_t size) {
        return kcalloc_vmm(1, size);
    }

    void MemoryObject::operator delete(void* ptr) {
        return kfree_vmm(ptr);
    }

    void MemoryObject::operator delete(void* ptr, size_t) {
        return kfree_vmm(ptr);
    }

    VMM::VMM() : m_pageMapper(nullptr), m_vmRegionAllocator(nullptr), m_mapEntries() {

    }

    VMM::VMM(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator) : m_pageMapper(pageMapper), m_vmRegionAllocator(vmRegionAllocator), m_mapEntries() {

    }

    VMM::~VMM() {

    }

    void VMM::Init(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator) {
        m_pageMapper = pageMapper;
        m_vmRegionAllocator = vmRegionAllocator;
    }

    void VMM::Delete() {
        m_mapEntriesLock.Lock();
        m_mapEntries.Drain([&](MapEntry* e) -> void {
            ReleaseEntry(e, false);
        });
        m_mapEntriesLock.Unlock();
    }

    void* VMM::AllocateAnonPages(uint64_t count, AllocFlags flags) {
        return AllocateAnonPages(count, nullptr, flags);
    }

    void* VMM::AllocateAnonPages(uint64_t count, void* addr, AllocFlags allocFlags) {
        if (count == 0 || g_defaultPager == nullptr)
            return nullptr;

        // Step 1: get a VM region. doing this first as it is more likely to fail (whilst still being quite unlikely), and easier to cleanup
        void* pages = nullptr;
        if (addr == nullptr)
            pages = m_vmRegionAllocator->AllocatePages(count);
        else {
            pages = m_vmRegionAllocator->AllocatePages(addr, count);
            if (pages == nullptr) {
                if (allocFlags.addrIsHint)
                    pages = m_vmRegionAllocator->AllocatePages(count);
                else if (allocFlags.replace) {
                    m_mapEntriesLock.Lock();
                    m_vmRegionAllocator->Lock();
                    bool rc = Internal_FreePages(addr, count, true, false);
                    m_mapEntriesLock.Unlock();
                    if (!rc) {
                        m_vmRegionAllocator->Unlock();
                        return nullptr;
                    }
                    pages = m_vmRegionAllocator->AllocatePages(addr, count, false);
                    m_vmRegionAllocator->Unlock();
                    if (pages == nullptr && allocFlags.addrIsHint)
                        pages = m_vmRegionAllocator->AllocatePages(count);
                }
            }
        }
        if (pages == nullptr)
            return nullptr;

        // Step 2: put together the anon map
        MemoryObject* anon = new MemoryObject;
        if (anon == nullptr) {
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        anon->size = count * PAGE_SIZE;
        anon->refCount = 1;
        anon->kind = ObjectKind::Anon;

        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)); // allocate this now for easier error handling
        if (entry == nullptr) {
            kfree_vmm(anon);
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }


        // Step 3: Allocate physical addresses if requested
        if (allocFlags.allocPhys) {
            bool ok = true;
            for (uint64_t i = 0; i < count && ok; i++) {
                Page* p = NewAnonPage(nullptr, allocFlags.zero);
                if (p == nullptr) {
                    ok = false;
                    break;
                }
                anon->pages.Insert(p, i * PAGE_SIZE);
            }
            if (!ok) {
                DestroyMemoryObject(anon);
                kfree_vmm(entry);
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }
            for (uint64_t i = 0; i < count; i++)
                m_pageMapper->MapPage(((uint64_t)pages + i * PAGE_SIZE), anon->pages.Find(i * PAGE_SIZE)->physAddr, allocFlags.protection, allocFlags.user, allocFlags.cacheType);
        }

        // Step 4: Build the map entry
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = (uint64_t)pages + count * PAGE_SIZE;
        entry->anon = anon; // offset of 0
        entry->flags.protection = allocFlags.protection;
        entry->flags.cacheType = allocFlags.cacheType;
        entry->flags.user = allocFlags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = allocFlags.isPrivate;
        entry->flags.zero = allocFlags.zero;

        m_mapEntriesLock.Lock();
        m_mapEntries.Insert(entry, (uint64_t)pages);
        m_mapEntriesLock.Unlock();

        if (allocFlags.allocPhys)
            m_pageMapper->InvalidatePages((uint64_t)pages, count);

        return pages;
    }

    void* VMM::AllocateBackedPages(uint64_t count, MemoryObject* obj, uint64_t offset, void* addr, AllocFlags allocFlags) {
        if (count == 0 || obj == nullptr)
            return nullptr;

        uint64_t page = offset >> PAGE_SIZE_SHIFT;

        obj->lock.Lock();
        bool inRange = (page + count) <= obj->size;
        obj->lock.Unlock();
        if (!inRange)
            return nullptr;

        // Step 1: get a VM region. doing this first as it is more likely to fail (whilst still being quite unlikely), and easier to cleanup
        void* pages = nullptr;
        if (addr == nullptr)
            pages = m_vmRegionAllocator->AllocatePages(count);
        else {
            pages = m_vmRegionAllocator->AllocatePages(addr, count);
            if (pages == nullptr) {
                if (allocFlags.addrIsHint)
                    pages = m_vmRegionAllocator->AllocatePages(count);
                else if (allocFlags.replace) {
                    m_mapEntriesLock.Lock();
                    m_vmRegionAllocator->Lock();
                    bool rc = Internal_FreePages(addr, count, true, false);
                    m_mapEntriesLock.Unlock();
                    if (!rc) {
                        m_vmRegionAllocator->Unlock();
                        return nullptr;
                    }
                    pages = m_vmRegionAllocator->AllocatePages(addr, count, false);
                    m_vmRegionAllocator->Unlock();
                    if (pages == nullptr && allocFlags.addrIsHint)
                        pages = m_vmRegionAllocator->AllocatePages(count);
                }
            }
        }
        if (pages == nullptr)
            return nullptr;

        // Step 2: Get a MapEntry
        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry));
        if (entry == nullptr) {
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        // Step 3: Get an anonymous MemoryObject if it is a private mapping
        MemoryObject* anon = nullptr;

        if (allocFlags.isPrivate) {
            anon = new MemoryObject;
            if (anon == nullptr) {
                kfree_vmm(entry);
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }

            anon->size = count * PAGE_SIZE;
            anon->refCount = 1;
            anon->kind = ObjectKind::Anon;
        }

        // Step 4: Allocate physical addresses if requested
        if (allocFlags.allocPhys) {
            obj->lock.Lock();
            for (uint64_t i = 0; i < count; i++) {
                Page* page = nullptr;
                if (!obj->pager->GetPage(obj, offset + i * PAGE_SIZE, &page, false) || page == nullptr || !EnsureResident(obj, page, allocFlags.zero)) {
                    obj->lock.Unlock();
                    if (allocFlags.isPrivate)
                        DestroyMemoryObject(anon);
                    kfree_vmm(entry);
                    m_vmRegionAllocator->FreePages(pages, count);
                    return nullptr;
                }
                m_pageMapper->MapPage(((uint64_t)pages + i * PAGE_SIZE), page->physAddr, WithoutWrite(allocFlags.protection), allocFlags.user, allocFlags.cacheType);
            }
            obj->lock.Unlock();
        }

        // Step 5: Build the MapEntry
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = entry->startVirt + count * PAGE_SIZE;
        entry->anon = anon; // offset 0
        entry->object = obj;
        entry->objectOffset = offset;
        entry->flags.protection = allocFlags.protection;
        entry->flags.cacheType = allocFlags.cacheType;
        entry->flags.user = allocFlags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = allocFlags.isPrivate;
        entry->flags.zero = false;

        RefMemObj(obj);

        m_mapEntriesLock.Lock();
        m_mapEntries.Insert(entry, (uint64_t)pages);
        m_mapEntriesLock.Unlock();

        if (allocFlags.allocPhys)
            m_pageMapper->InvalidatePages((uint64_t)pages, count);

        return pages;
    }

    void* VMM::AllocMemObjAnonPages(uint64_t count, void* pagerData, uint64_t offset, AllocFlags flags, MemoryObject** objOut, DefaultPager* pager) {
        if (count == 0 || pagerData == nullptr)
            return nullptr;

        // Step 1: get a VM region. doing this first as it is more likely to fail (whilst still being quite unlikely), and easier to cleanup
        void* pages = m_vmRegionAllocator->AllocatePages(count);
        if (pages == nullptr)
            return nullptr;

        // Step 2: put together the memory object
        bool createdObj = false;
        MemoryObject* obj = nullptr;
        if (objOut != nullptr)
            obj = *objOut;
        if (obj == nullptr) {
            obj = new MemoryObject;
            if (obj == nullptr) {
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }
            createdObj = true;

            obj->pager = pager;
            obj->pagerData = pagerData;
            obj->size = count;
            obj->refCount = 1;
            obj->kind = ObjectKind::Backed;
        } else {
            RefMemObj(obj);
            obj->lock.Lock();
            obj->size += count;
        }

        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)); // allocate this now for easier error handling
        if (entry == nullptr) {
            if (UnrefMemObj(obj))
                DestroyMemoryObject(obj);
            else
                obj->lock.Unlock();
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        // Step 3: build the page list
        if (flags.allocPhys) {
            for (uint64_t i = 0; i < count; i++) {
                Page* page = (Page*)kcalloc_vmm(1, sizeof(Page)); // TODO: check if page is nullptr
                page->protection = flags.protection;
                page->isWired = flags.allocPhys;
                page->physAddr = (uint64_t)g_PMM->AllocatePage();
                if (flags.zero)
                    memset((void*)to_HHDM(page->physAddr), 0, PAGE_SIZE);
                m_pageMapper->MapPage((uint64_t)pages + i * PAGE_SIZE, page->physAddr, flags.protection, flags.user, flags.cacheType); // TODO: check failure
                obj->pages.Insert(page, offset + i * PAGE_SIZE);
            }
            
        }

        // Step 4: build the map entry
        entry->object = obj;
        entry->objectOffset = offset;
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = (uint64_t)pages + count * PAGE_SIZE;
        entry->flags.protection = flags.protection;
        entry->flags.cacheType = flags.cacheType;
        entry->flags.user = flags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = flags.isPrivate;
        entry->flags.zero = flags.zero;

        if (!createdObj)
            obj->lock.Unlock();

        m_mapEntriesLock.Lock();
        m_mapEntries.Insert(entry, (uint64_t)pages);
        m_mapEntriesLock.Unlock();

        if (objOut != nullptr)
            *objOut = obj;

        if (flags.allocPhys)
            m_pageMapper->InvalidatePages((uint64_t)pages, count);

        return pages;
    }

    bool VMM::FreePages(void* virtAddr, uint64_t count, bool multipleRegions) {
        return Internal_FreePages(virtAddr, count, multipleRegions, true);
    }

    bool VMM::RemapPages(void* virtAddr, uint64_t totalCount, Protection prot, bool user, CacheType cacheType, bool multipleRegions) {
        uint64_t virt = reinterpret_cast<uint64_t>(virtAddr);

        bool full = false;
        if (totalCount == 0) {
            full = true;
            totalCount = 1;
            if (multipleRegions)
                return false;
        }
        
        if (m_vmRegionAllocator == nullptr || virt < m_vmRegionAllocator->GetStart() || (virt + totalCount * PAGE_SIZE) > m_vmRegionAllocator->GetEnd())
            return false; // outside the region
        
        bool first = true;
        uint64_t currentCount = 0;
        bool shootdown = false;

        while (currentCount < totalCount) {
            m_mapEntriesLock.Lock();

            uint64_t count = 1;
            if (first && !multipleRegions)
                count = totalCount;

            MapEntry* entry = full ? m_mapEntries.Find(virt) : m_mapEntries.FindLE(virt);
            if (entry == nullptr || (full && entry->startVirt != virt) || (!full && entry->endVirt < virt + count * PAGE_SIZE)) {
                m_mapEntriesLock.Unlock();
                return false;
            }

            if (!first && multipleRegions)
                count = MIN((entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT, totalCount - currentCount);

            if (!full && (entry->startVirt != virt || entry->endVirt != virt + count * PAGE_SIZE)) {
                uint64_t start = entry->startVirt;
                uint64_t end = entry->endVirt;
                bool needLower = entry->startVirt < virt;
                bool needUpper = entry->endVirt > virt + count * PAGE_SIZE;
                MapEntry* spares[2] = {needLower ? (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)) : nullptr, needUpper ? (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)) : nullptr,};
                if ((needLower && spares[0] == nullptr) || (needUpper && spares[1] == nullptr)) {
                    m_mapEntriesLock.Unlock();
                    return false;
                }
                if (needLower) {
                    MapEntry* newEntry = SplitMapEntry(entry, (virt - entry->startVirt) >> PAGE_SIZE_SHIFT, spares[0]);
                    m_mapEntries.Insert(newEntry, newEntry->startVirt);
                    entry = newEntry;
                }
                if (needUpper) {
                    MapEntry* newEntry = SplitMapEntry(entry, count, spares[1]);
                    m_mapEntries.Insert(newEntry, newEntry->startVirt);
                }
                bool rc = m_vmRegionAllocator->ResizeAllocatedRegion((void*)start, (end - start) >> PAGE_SIZE_SHIFT, (void*)virt, count);
                assert(rc);
            } else if (full)
                count = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;

            MemoryObject* anon = entry->anon;
            MemoryObject* obj = entry->object;

            if (anon != nullptr) {
                anon->lock.Lock();

                if (obj != nullptr) {
                    obj->lock.Lock();
                    bool valid = true;
                    obj->pages.ForEachFrom([&](uint64_t offset, Page* page) -> bool {
                        if (offset >= entry->objectOffset + (entry->endVirt - entry->startVirt))
                            return false;
                        if (!isLessOrEqualProt(prot, page->protection)) {
                            valid = false;
                            return false;
                        }
                        return true;
                    }, entry->objectOffset);
                    if (!valid) {
                        obj->lock.Unlock();
                        anon->lock.Unlock();
                        m_mapEntriesLock.Unlock();
                        return false;
                    }
                }

                const bool priv = entry->flags.isPrivate;
                const bool needsCopy = entry->flags.needsCopy;
                const Protection roProt = WithoutWrite(prot);
                const uint64_t len = entry->endVirt - entry->startVirt;
                
                // Now that it is confirmed to be valid, we can remap
                if (anon != nullptr) {
                    anon->pages.ForEachFrom([&](uint64_t off, Page* p) -> bool {
                        if (off >= entry->anonOffset + len)
                            return false;
                        bool ro = priv && (needsCopy || PageIsShared(p));
                        m_pageMapper->RemapPage(entry->startVirt + (off - entry->anonOffset), ro ? roProt : prot, user, cacheType);
                        return true;
                    }, entry->anonOffset);
                }

                if (obj != nullptr) {
                    obj->pages.ForEachFrom([&](uint64_t off, Page* p) -> bool {
                        if (off >= entry->objectOffset + len)
                            return false;
                        uint64_t rel = off - entry->objectOffset;
                        if (p->physAddr == 0 || (anon != nullptr && anon->pages.Find(entry->anonOffset + rel) != nullptr))
                            return true;
                        m_pageMapper->RemapPage(entry->startVirt + rel, (anon != nullptr && priv) ? roProt : prot, user, cacheType);
                        return true;
                    }, entry->objectOffset);
                    obj->lock.Unlock();
                }

                anon->lock.Unlock();
            } else if (obj != nullptr) {
                obj->lock.Lock();
                bool valid = true;
                obj->pages.ForEachFrom([&](uint64_t offset, Page* page) -> bool {
                    if (offset >= entry->objectOffset + (entry->endVirt - entry->startVirt))
                        return false;
                    if (!isLessOrEqualProt(prot, page->protection)) {
                        valid = false;
                        return false;
                    }
                    return true;
                }, entry->objectOffset);
                if (!valid) {
                    obj->lock.Unlock();
                    m_mapEntriesLock.Unlock();
                    return false;
                }

                obj->pages.ForEachFrom([&](uint64_t offset, Page* page) -> bool {
                    if (offset >= entry->objectOffset + (entry->endVirt - entry->startVirt))
                        return false;
                    if (page->physAddr == 0)
                        return true;
                    m_pageMapper->RemapPage(virt + offset - entry->objectOffset, prot, user, cacheType);
                    return true;
                }, entry->objectOffset);

                obj->lock.Unlock();
            }

            Protection oldProt = entry->flags.protection;
            bool wasUser = entry->flags.user;

            entry->flags.protection = prot;
            entry->flags.user = user;

            m_mapEntriesLock.Unlock(); // need to hold the lock for the whole function to ensure it can't be unmapped on us part way through

            shootdown |= (user ^ wasUser ) || m_pageMapper->isPermsReduction(oldProt, prot);

            currentCount += count;
            first = false;
            virt += count * PAGE_SIZE;
        }

        m_pageMapper->InvalidatePages((uint64_t)virtAddr, totalCount, shootdown);
        return true;
    }

    bool VMM::MapPages(void* virtAddr, uint64_t count) {
        m_mapEntriesLock.Lock();

        MapEntry* entry = m_mapEntries.FindLE((uint64_t)virtAddr);
        if (entry == nullptr || entry->endVirt <= (uint64_t)virtAddr || ((uint64_t)virtAddr + count * PAGE_SIZE) > entry->endVirt || entry->anon == nullptr) {
            m_mapEntriesLock.Unlock();
            return false;
        }
        MemoryObject* anon = entry->anon;

        anon->lock.Lock();

        uint64_t first = ((uint64_t)virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT;
        for (uint64_t i = first; i < first + count; i++) {
            Page* p = NewAnonPage(nullptr, entry->flags.zero);
            if (p == nullptr) {
                anon->lock.Unlock();
                m_mapEntriesLock.Unlock();
                return false;
            }
            if (!anon->pages.Insert(p, entry->anonOffset + i * PAGE_SIZE)) {
                ReleasePage(anon, p);
                continue;
            }
            m_pageMapper->MapPage(entry->startVirt + i * PAGE_SIZE, p->physAddr, entry->flags.protection, entry->flags.user, entry->flags.cacheType);
        }

        anon->lock.Unlock();
        
        m_mapEntriesLock.Unlock();
        
        m_pageMapper->InvalidatePages((uint64_t)virtAddr, count, false); // Not a permission reduction, shootdown is not required
        return true;
    }

    static bool FaultMatchesProt(Protection prot, PageFaultCode c) {
        switch (prot) {
        case Protection::READ:
            return !c.write && !c.execute;
            break;
        case Protection::WRITE:
            return c.write && !c.execute;
            break;
        case Protection::EXECUTE:
            return c.execute;
            break;
        case Protection::READ_WRITE:
            return !c.execute;
            break;
        case Protection::READ_EXECUTE:
            return !c.write;
            break;
        case Protection::READ_WRITE_EXECUTE:
            return true;
            break;
        }
        return false;
    }

    bool VMM::HandlePageFault(PageFaultCode code, uint64_t virtAddr, bool* busError) {
        if (busError != nullptr)
            *busError = false;
        if (m_vmRegionAllocator == nullptr || virtAddr < m_vmRegionAllocator->GetStart() || virtAddr >= m_vmRegionAllocator->GetEnd())
            return false; // outside the region

        m_mapEntriesLock.Lock();
        MapEntry* entry = m_mapEntries.FindLE(virtAddr);
        if (entry == nullptr || virtAddr >= entry->endVirt || (entry->anon == nullptr && entry->object == nullptr) || !FaultMatchesProt(entry->flags.protection, code)) {
            m_mapEntriesLock.Unlock();
            return false;
        }

        if (code.write && entry->flags.needsCopy && !ResolveNeedsCopy(entry)) {
            m_mapEntriesLock.Unlock();
            return false;
        }

        MemoryObject* anon = entry->anon;
        MemoryObject* obj = entry->object;
        virtAddr = ALIGN_DOWN(virtAddr, PAGE_SIZE);
        const uint64_t rel = virtAddr - entry->startVirt;
        const uint64_t anonKey = entry->anonOffset + rel;
        uint64_t objKey = entry->objectOffset + rel;
        const Protection prot = entry->flags.protection;
        const CacheType cacheType = entry->flags.cacheType;
        const bool user = entry->flags.user;
        const bool zero = entry->flags.zero;
        const bool isPrivate = entry->flags.isPrivate;
        const bool needsCopy = entry->flags.needsCopy;

        // Lock handoff
        (anon != nullptr ? anon->lock : obj->lock).Lock();
        m_mapEntriesLock.Unlock();

        bool result = false;
        bool bus = false;

        if (anon == nullptr) {
            Page* op = nullptr;
            if (!obj->pager->GetPage(obj, objKey, &op, code.write) || op == nullptr)
                bus = true;
            else if (EnsureResident(obj, op, zero))
                result = code.present ? m_pageMapper->RemapPage(virtAddr, prot, user, cacheType)
                                      : m_pageMapper->MapPage(virtAddr, op->physAddr, prot, user, cacheType);
            obj->lock.Unlock();
            if (busError != nullptr)
                *busError = bus;
            return result;
        }

        Page* ap = anon->pages.Find(anonKey);
        if (ap != nullptr) {
            const bool needsCow = isPrivate && (needsCopy || PageIsShared(ap));
            if (code.write && needsCow) {
                Page* np = NewAnonPage(ap, false);
                if (np != nullptr) {
                    bool replaced = m_pageMapper->GetPhysicalAddr(virtAddr != 0);
                    if (m_pageMapper->MapPage(virtAddr, np->physAddr, prot, user, cacheType)) {
                        anon->pages.Remove(ap);
                        anon->pages.Insert(np, anonKey);
                        if (replaced)
                            m_pageMapper->InvalidatePages(virtAddr, 1, true);
                        ReleasePage(anon, ap);
                        result = true;
                    } else
                        ReleasePage(anon, np);
                }
            } else {
                bool replaced = m_pageMapper->GetPhysicalAddr(virtAddr != 0);
                const Protection mp = needsCow ? WithoutWrite(prot) : prot;
                result = code.present ? m_pageMapper->RemapPage(virtAddr, mp, user, cacheType)
                                      : m_pageMapper->MapPage(virtAddr, ap->physAddr, mp, user, cacheType);
                if (replaced)
                    m_pageMapper->InvalidatePages(virtAddr, 1, true);
            }
        } else if (obj != nullptr) {
            obj->lock.Lock();
            Page* op = nullptr;
            if (!obj->pager->GetPage(obj, objKey, &op, code.write) || op == nullptr)
                bus = true;
            else if (EnsureResident(obj, op, zero)) {
                if (code.write) {
                    Page* np = NewAnonPage(op, false);
                    if (np != nullptr) {
                        bool replaced = m_pageMapper->GetPhysicalAddr(virtAddr != 0);
                        if (m_pageMapper->MapPage(virtAddr, np->physAddr, prot, user, cacheType)) {
                            anon->pages.Insert(np, anonKey);
                            if (replaced)
                                m_pageMapper->InvalidatePages(virtAddr, 1, true);
                            result = true;
                        } else
                            ReleasePage(anon, np);
                    }
                } else {
                    bool replaced = m_pageMapper->GetPhysicalAddr(virtAddr != 0);
                    result = m_pageMapper->MapPage(virtAddr, op->physAddr, WithoutWrite(prot), user, cacheType);
                    if (replaced)
                        m_pageMapper->InvalidatePages(virtAddr, 1, true);
                }
            }
            obj->lock.Unlock();
        } else {
            Page* np = NewAnonPage(nullptr, zero);
            if (np != nullptr) {
                const Protection mp = (isPrivate && needsCopy) ? WithoutWrite(prot) : prot;
                bool replaced = m_pageMapper->GetPhysicalAddr(virtAddr != 0);
                if (m_pageMapper->MapPage(virtAddr, np->physAddr, mp, user, cacheType)) {
                    anon->pages.Insert(np, anonKey);
                    if (replaced)
                        m_pageMapper->InvalidatePages(virtAddr, 1, true);
                    result = true;
                } else
                    ReleasePage(anon, np);
            }
        }

        anon->lock.Unlock();
        if (busError != nullptr)
            *busError = bus;
        return result;
    }

    bool VMM::PrepareAccess(const void* addr, size_t size, bool write, bool user) {
        if (size == 0)
            return true;
        if (!ValidateAccess(addr, size, write ? Protection::WRITE : Protection::READ, user))
            return false;

        uint64_t end = (uint64_t)addr + size;
        for (uint64_t virt = ALIGN_DOWN((uint64_t)addr, PAGE_SIZE); virt < end; virt += PAGE_SIZE) {
            if (PageIsReady(virt, write))
                continue;
            // Simulate a page fault to force the memory to be paged in
            PageFaultCode code = { .present = false, .write = write, .user = user, .execute = false };
            if (!HandlePageFault(code, virt))
                return false;
        }
        return true;
    }

    bool VMM::ValidateRead(const void* addr, size_t size, bool user) {
        return ValidateAccess(addr, size, Protection::READ, user);
    }

    bool VMM::ValidateWrite(const void* addr, size_t size, bool user) {
        return ValidateAccess(addr, size, Protection::WRITE, user);
    }

    bool VMM::CopyStringFromUser(char* kernelDst, const void* userSrc, size_t maxSize, size_t* outSize, bool user) {
        uint64_t virtAddr = reinterpret_cast<uint64_t>(userSrc);
        size_t totalCopied = 0;

        if (maxSize == 0) {
            if (outSize != nullptr)
                *outSize = 0;
            return false;
        }

        m_mapEntriesLock.Lock();

        while (totalCopied < maxSize) {
            MapEntry* entry = m_mapEntries.FindLE(virtAddr);
            if (entry == nullptr) {
                m_mapEntriesLock.Unlock();
                if (outSize != nullptr)
                    *outSize = totalCopied;
                return false; // Unmapped memory hit
            }

            Protection prot = entry->flags.protection;

            // Verify bounds and READ permissions
            if (virtAddr < entry->startVirt || virtAddr >= entry->endVirt || (user && !entry->flags.user) || (static_cast<uint8_t>(prot) & static_cast<uint8_t>(Protection::READ)) == 0) {
                m_mapEntriesLock.Unlock();
                if (outSize != nullptr)
                    *outSize = totalCopied;
                return false; // Access violation
            }

            uint64_t pageIndex = (virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT;
            uint64_t pageOffset = virtAddr & (PAGE_SIZE - 1);
            size_t bytesInPage = PAGE_SIZE - pageOffset;
            size_t bytesInRegion = entry->endVirt - virtAddr;
            size_t bytesToProcess = MIN(bytesInPage, MIN(bytesInRegion, maxSize - totalCopied));

            const char* srcHHDM = nullptr;
            bool isZeroPage = false;

            // Step 1: Resolve HHDM pointer or demand-zero state
            if (entry->anon != nullptr) {
                MemoryObject* anon = entry->anon;
                anon->lock.Lock();
                Page * p = anon->pages.Find(pageIndex * PAGE_SIZE + entry->anonOffset);
                if (p != nullptr && p->physAddr != 0)
                    srcHHDM = reinterpret_cast<const char*>(to_HHDM(p->physAddr)) + pageOffset;
                anon->lock.Unlock();

                if (srcHHDM == nullptr && entry->object != nullptr) { // private file page not yet copied
                    MemoryObject* obj = entry->object;
                    obj->lock.Lock();
                    Page* page = obj->pages.Find(entry->objectOffset + pageIndex * PAGE_SIZE);
                    if (page != nullptr && page->physAddr != 0)
                        srcHHDM = reinterpret_cast<const char*>(to_HHDM(page->physAddr)) + pageOffset;
                    obj->lock.Unlock();
                }
                if (srcHHDM == nullptr && entry->flags.zero)
                    isZeroPage = true;
            } else if (entry->object != nullptr) {
                MemoryObject* obj = entry->object;
                obj->lock.Lock();
                Page* page = obj->pages.Find(entry->objectOffset + pageIndex * PAGE_SIZE);
                if (page != nullptr && page->physAddr != 0)
                    srcHHDM = reinterpret_cast<const char*>(to_HHDM(page->physAddr)) + pageOffset;
                else if (entry->flags.zero)
                    isZeroPage = true;
                obj->lock.Unlock();
            }

            // Step 2: Handle unpopulated file-backed pages safely
            if (srcHHDM == nullptr && !isZeroPage) {
                // Memory is mapped but frame isn't in RAM yet (e.g., pager needs to load it).
                // Unlock m_mapEntries, trigger Page Fault to bring frame into RAM, then retry.
                m_mapEntriesLock.Unlock();
                PageFaultCode code = { .present = false, .write = false, .user = user, .execute = false };
                if (!HandlePageFault(code, virtAddr)) {
                    if (outSize != nullptr)
                        *outSize = totalCopied;
                    return false; // Pager failed to load page
                }
                m_mapEntriesLock.Lock();
                continue; // Retry this address now that it's paged in
            }

            // Step 3: Copy & scan bytes
            if (isZeroPage) {
                // Demand-zero page: first byte is '\0'
                kernelDst[totalCopied] = '\0';
                m_mapEntriesLock.Unlock();
                if (outSize != nullptr)
                    *outSize = totalCopied + 1;
                return true; // Successfully found null terminator!
            }

            for (size_t i = 0; i < bytesToProcess; ++i) {
                char c = srcHHDM[i];
                kernelDst[totalCopied] = c;
                totalCopied++;

                if (c == '\0') {
                    m_mapEntriesLock.Unlock();
                    if (outSize != nullptr)
                        *outSize = totalCopied;
                    return true; // Null terminator reached
                }
            }

            virtAddr += bytesToProcess;
        }

        m_mapEntriesLock.Unlock();

        // Reached maxSize without seeing '\0'
        if (outSize != nullptr)
            *outSize = totalCopied;
        return false;
    }

    bool VMM::Fork(VMM* other) {
        other->m_mapEntriesLock.Lock();
        m_mapEntriesLock.Lock();
        bool success = true;
        other->m_mapEntries.ForEach([&](uint64_t virt, MapEntry* e) -> bool {
            MapEntry* c = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry));
            if (c == nullptr) {
                success = false;
                return false;
            }

            c->startVirt = e->startVirt;
            c->endVirt = e->endVirt;
            c->anon = e->anon;
            c->anonOffset = e->anonOffset;
            c->object = e->object;
            c->objectOffset = e->objectOffset;

            if (e->anon != nullptr)
                RefMemObj(e->anon);

            if (e->object != nullptr)
                RefMemObj(e->object);

            if (e->anon != nullptr && e->flags.isPrivate) {
                e->flags.needsCopy = true;
                if ((static_cast<uint8_t>(e->flags.protection) & static_cast<uint8_t>(Protection::WRITE))) {
                    Protection ro = WithoutWrite(e->flags.protection);
                    uint64_t lo = UINT64_MAX;
                    uint64_t hi = 0;
                    e->anon->lock.Lock();
                    e->anon->pages.ForEachFrom([&](uint64_t off, Page*) -> bool {
                        if (off >= e->anonOffset + (e->endVirt - e->startVirt))
                            return false;
                        uint64_t rel = off - e->anonOffset;
                        other->m_pageMapper->RemapPage(e->startVirt + rel, ro, e->flags.user, e->flags.cacheType);
                        if ((rel >> PAGE_SIZE_SHIFT) < lo)
                            lo = rel >> PAGE_SIZE_SHIFT;
                        if ((rel >> PAGE_SIZE_SHIFT) > hi)
                            hi = rel >> PAGE_SIZE_SHIFT;
                        return true;
                    }, e->anonOffset);
                    e->anon->lock.Unlock();
                    if (lo != UINT64_MAX)
                        other->m_pageMapper->InvalidatePages(e->startVirt + (lo << PAGE_SIZE_SHIFT), hi - lo + 1, true);
                }
            }

            c->flags = e->flags;
            m_mapEntries.Insert(c, c->startVirt);
            return true;
        });
        m_mapEntriesLock.Unlock();
        other->m_mapEntriesLock.Unlock();
        return success;
    }

    PageMapper* VMM::GetPageMapper() {
        return m_pageMapper;
    }

    VMRegionAllocator* VMM::GetAllocator() {
        return m_vmRegionAllocator;
    }

    void VMM::DumpRegions(fd_t fd) {
        fprintf(fd, "\nMap Entries:\n");
        m_mapEntriesLock.Lock();
        m_mapEntries.ForEach([fd](uint64_t key, MapEntry* entry) -> bool {
            fprintf(fd, "Entry: %lx-%lx, offset = %lx, anonMap = %p, memoryObject = %p, Flags:\n\tprot = %x\n\tcacheType = %x\n\tuser = %s, needsCopy = %s, isPrivate = %s, zero = %s\n", entry->startVirt, entry->endVirt, entry->objectOffset, entry->anon, entry->object, entry->flags.protection, entry->flags.cacheType, entry->flags.user ? "true" : "false", entry->flags.needsCopy ? "true" : "false", entry->flags.isPrivate ? "true" : "false", entry->flags.zero ? "true" : "false");
            if (entry->anon != nullptr) {
                MemoryObject* anon = entry->anon;
                anon->lock.Lock();
                fprintf(fd, "Anon MemoryObject: size = %lx refCount = %lu\n", anon->size, anon->refCount);
                anon->lock.Unlock();
            }
            if (entry->object != nullptr) {
                MemoryObject* obj = entry->object;
                obj->lock.Lock();
                fprintf(fd, "MemoryObject: size = %lx, refCount = %lu, pager = %p, pagerData = %p\n", obj->size, obj->refCount, obj->pager, obj->pagerData);
                obj->lock.Unlock();
            }
            fputc(fd, '\n');
            return true;
        });
        m_mapEntriesLock.Unlock();
        fputc(fd, '\n');
    }

    MapEntry* VMM::SplitMapEntry(MapEntry* entry, uint64_t lowerPages, MapEntry* spare) {
        spare->startVirt = entry->startVirt + lowerPages * PAGE_SIZE;
        spare->endVirt = entry->endVirt;
        spare->flags = entry->flags;

        if (entry->anon != nullptr) {
            spare->anonOffset = entry->anonOffset + lowerPages * PAGE_SIZE;
            spare->anon = entry->anon;
            RefMemObj(spare->anon);
        }
        if (entry->object != nullptr) {
            spare->objectOffset = entry->objectOffset + lowerPages * PAGE_SIZE;
            spare->object = entry->object;
            RefMemObj(spare->object);
        }

        entry->endVirt = spare->startVirt;
        return spare;
    }

    bool VMM::Internal_FreePages(void* virtAddr, uint64_t totalCount, bool multipleRegions, bool lock) {
        uint64_t virt = reinterpret_cast<uint64_t>(virtAddr);

        bool full = false;
        if (totalCount == 0) {
            full = true;
            totalCount = 1;
            if (multipleRegions)
                return false;
        }

        if (m_vmRegionAllocator == nullptr || virt < m_vmRegionAllocator->GetStart() || (virt + totalCount * PAGE_SIZE) > m_vmRegionAllocator->GetEnd())
            return false; // outside the region

        bool first = true;
        uint64_t currentCount = 0;

        while (currentCount < totalCount) {
            if (lock)
                m_mapEntriesLock.Lock();

            uint64_t count = 1;
            if (first && !multipleRegions)
                count = totalCount;

            MapEntry* entry = full ? m_mapEntries.Find(virt) : m_mapEntries.FindLE(virt);
            if (entry == nullptr || (full && entry->startVirt != virt) || (!full && entry->endVirt < virt + count * PAGE_SIZE)) {
                if (lock)
                    m_mapEntriesLock.Unlock();
                return false;
            }

            if (!first && multipleRegions)
                count = MIN((entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT, totalCount - currentCount);

            if (!full && (entry->startVirt != virt || entry->endVirt != virt + count * PAGE_SIZE)) {
                bool needLower = entry->startVirt < virt;
                bool needUpper = entry->endVirt > virt + count * PAGE_SIZE;
                MapEntry* spares[2] = {needLower ? (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)) : nullptr, needUpper ? (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)) : nullptr,};
                if ((needLower && spares[0] == nullptr) || (needUpper && spares[1] == nullptr)) {
                    if (lock)
                        m_mapEntriesLock.Unlock();
                    return false;
                }
                if (needLower)
                    entry = SplitMapEntry(entry, (virt - entry->startVirt) >> PAGE_SIZE_SHIFT, spares[0]);
                else
                    m_mapEntries.Remove(entry);
                if (needUpper) {
                    MapEntry* newEntry = SplitMapEntry(entry, count, spares[1]);
                    m_mapEntries.Insert(newEntry, newEntry->startVirt);
                }
            } else {
                if (full)
                    count = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;
                m_mapEntries.Remove(entry);
            }

            if (lock)
                m_mapEntriesLock.Unlock();

            m_vmRegionAllocator->FreePages(reinterpret_cast<void*>(virt), count, full, lock);

            ReleaseEntry(entry, true);

            first = false;
            currentCount += count;
            virt += count * PAGE_SIZE;
        }

        return true;
    }

    bool VMM::ValidateAccess(const void* addr, size_t size, Protection need, bool user) {
        uint64_t virt = (uint64_t)addr;
        if (size == 0)
            return true;
        if (size > UINT64_MAX - virt)
            return false;
        uint64_t end = virt + size;
        uint8_t needBits = static_cast<uint8_t>(need);

        m_mapEntriesLock.Lock();
        bool ok = true;
        while (virt < end) {
            MapEntry* e = m_mapEntries.FindLE(virt);
            if (e == nullptr || virt >= e->endVirt || (user && !e->flags.user) || (static_cast<uint8_t>(e->flags.protection) & needBits) != needBits) {
                ok = false;
                break;
            }
            virt = e->endVirt;
        }
        m_mapEntriesLock.Unlock();
        return ok;
    }

    bool VMM::PageIsReady(uint64_t virt, bool write) {
        m_mapEntriesLock.Lock();
        MapEntry* e = m_mapEntries.FindLE(virt);
        if (e == nullptr || virt >= e->endVirt) {
            m_mapEntriesLock.Unlock();
            return false;
        }
        uint64_t rel = virt - e->startVirt;
        bool ready = false;

        if (e->anon != nullptr) {
            e->anon->lock.Lock();
            Page* p = e->anon->pages.Find(e->anonOffset + rel);
            ready = p != nullptr && p->physAddr != 0 && (!write || !(e->flags.isPrivate && (e->flags.needsCopy || PageIsShared(p))));
            e->anon->lock.Unlock();
        }

        if (!ready && e->object != nullptr && (e->anon == nullptr || !write)) {
            e->object->lock.Lock();
            Page* p = e->object->pages.Find(e->objectOffset + rel);
            ready = p != nullptr && p->physAddr != 0;
            e->object->lock.Unlock();
        }
        m_mapEntriesLock.Unlock();
        return ready;
    }

    bool VMM::ResolveNeedsCopy(MapEntry* e) {
        MemoryObject* old = e->anon;
        if (old == nullptr || !MemObjIsShared(old)) {
            e->flags.needsCopy = false;
            return true;
        }

        const uint64_t len = e->endVirt - e->startVirt;
        MemoryObject* fresh = new MemoryObject;
        if (fresh == nullptr)
            return false;
        fresh->size = len >> PAGE_SIZE_SHIFT;
        fresh->refCount = 1;
        fresh->kind = ObjectKind::Anon;

        bool ok = true;
        old->lock.Lock();
        old->pages.ForEachFrom([&](uint64_t off, Page* p) -> bool {
            if (off >= e->anonOffset + len)
                return false;
            Page* np = (Page*)kcalloc_vmm(1, sizeof(Page));
            if (np == nullptr) {
                ok = false;
                return false;
            }
            np->physAddr = p->physAddr;
            np->protection = p->protection;
            FrameRef(np->physAddr);
            fresh->pages.Insert(np, off - e->anonOffset);
            return true;
        }, e->anonOffset);
        old->lock.Unlock();

        if (!ok) {
            DestroyMemoryObject(fresh);
            return false;
        }

        e->anon = fresh;
        e->anonOffset = 0;
        e->flags.needsCopy = false;

        old->lock.Lock();
        if (UnrefMemObj(old))
            DestroyMemoryObject(old);
        else
            old->lock.Unlock();
        return true;
    }

    void VMM::ReleaseEntry(MapEntry* e, bool invalidate) {
        const uint64_t len = e->endVirt - e->startVirt;
        const bool exclusive = e->flags.isPrivate && !e->flags.needsCopy;
        MemoryObject* anon = e->anon;
        MemoryObject* obj = e->object;
        uint64_t lo = UINT64_MAX;
        uint64_t hi = 0;
        auto unmap = [&](uint64_t rel) {
            uint64_t idx = rel >> PAGE_SIZE_SHIFT;
            if (idx < lo)
                lo = idx;
            if (idx > hi)
                hi = idx;
            m_pageMapper->UnmapPage(e->startVirt + rel);
        };

        if (anon != nullptr) {
            anon->lock.Lock();
            anon->pages.ForEachFrom([&](uint64_t off, Page*) -> bool {
                if (off >= e->anonOffset + len)
                    return false;
                unmap(off - e->anonOffset);
                return true;
            }, e->anonOffset);
            anon->lock.Unlock();
        }
        if (obj != nullptr) {
            obj->lock.Lock();
            obj->pages.ForEachFrom([&](uint64_t off, Page* p) -> bool {
                if (off >= e->objectOffset + len)
                    return false;
                if (p->physAddr != 0)
                    unmap(off - e->objectOffset);
                return true;
            }, e->objectOffset);
            obj->lock.Unlock();
        }
        if (invalidate && lo != UINT64_MAX)
            m_pageMapper->InvalidatePages(e->startVirt + (lo << PAGE_SIZE_SHIFT), hi - lo + 1, true);

        if (anon != nullptr) {
            anon->lock.Lock();
            if (exclusive) {
                anon->pages.ForEachFromSafe([&](uint64_t off, Page* p) -> bool {
                    if (off >= e->anonOffset + len)
                        return false;
                    anon->pages.Remove(p);
                    ReleasePage(anon, p);
                    return true;
                }, e->anonOffset);
            }
            if (UnrefMemObj(anon))
                DestroyMemoryObject(anon);
            else
                anon->lock.Unlock();
        }
        if (obj != nullptr) {
            obj->lock.Lock();
            if (UnrefMemObj(obj))
                DestroyMemoryObject(obj);
            else
                obj->lock.Unlock();
        }
        kfree_vmm(e);
    }


    void ReleaseMemoryObject(MemoryObject* obj) {
        if (UnrefMemObj(obj))
            DestroyMemoryObject(obj);
    }
}