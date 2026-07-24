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

    bool isLessOrEqualProt(Protection a, Protection b) {
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

    VMM::VMM() : m_pageMapper(nullptr), m_vmRegionAllocator(nullptr), m_mapEntries(true) {

    }

    VMM::VMM(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator) : m_pageMapper(pageMapper), m_vmRegionAllocator(vmRegionAllocator), m_mapEntries(true) {

    }

    VMM::~VMM() {

    }

    void VMM::Init(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator) {
        m_pageMapper = pageMapper;
        m_vmRegionAllocator = vmRegionAllocator;
    }

    void VMM::Delete() {
        m_mapEntries.lock();
        
        m_mapEntries.forEach([](void* data, uint64_t, MapEntry* entry) -> bool {
            VMM* current = (VMM*)data;
            AnonMap* map = entry->anonMap;
            MemoryObject* obj = entry->memoryObject;
            if (map != nullptr) {
                spinlock_acquire(&map->lock);

                map->refCount--;

                for (uint64_t i = 0; i < map->slotCount; i++) {
                    Anon* anon = map->slots[i];
                    if (anon != nullptr) {
                        if (anon->physAddr != 0)
                            current->m_pageMapper->UnmapPage(entry->startVirt + i * PAGE_SIZE);
                        anon->refCount--;
                        if (anon->refCount == 0) {
                            g_PMM->FreePage((void*)anon->physAddr);
                            kfree_vmm(anon);
                        }
                    }
                }
                
                if (map->refCount == 0) {
                    kfree_vmm(map->slots);
                    kfree_vmm(map);
                } else
                    spinlock_release(&map->lock);
            }
            if (obj != nullptr) {
                spinlock_acquire(&obj->lock);
                obj->refCount--;
                struct Data {
                    VMM* vmm;
                    MapEntry* entry;
                    bool del;
                } data = {current, entry, obj->refCount == 0};
                obj->pages.forEach([](void* data, uint64_t offset, Page* page) -> void {
                    Data* d = (Data*)data;
                    if (page->physAddr != 0) {
                        d->vmm->m_pageMapper->UnmapPage(d->entry->startVirt + (offset - d->entry->offset) * PAGE_SIZE);
                        if (d->del)
                            g_PMM->FreePage((void*)page->physAddr);
                    }
                    if (d->del)
                        kfree_vmm(page);
                    }, &data);
                if (obj->refCount == 0)
                    kfree_vmm(obj);
                else
                    spinlock_release(&obj->lock);
            }
            kfree_vmm(entry);
            return true;
        }, this);

        m_mapEntries.Clear();
        m_mapEntries.unlock();
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
                    m_mapEntries.lock();
                    m_vmRegionAllocator->Lock();
                    bool rc = Internal_FreePages(addr, count, true, false);
                    m_mapEntries.unlock();
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
        AnonMap* map = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
        if (map == nullptr) {
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        map->slotCount = count;
        map->refCount = 1;
        map->slots = (Anon**)kcalloc_vmm(count, sizeof(Anon*));
        if (map->slots == nullptr) {
            kfree_vmm(map);
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)); // allocate this now for easier error handling
        if (entry == nullptr) {
            kfree_vmm(map->slots);
            kfree_vmm(map);
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }


        // Step 3: Allocate physical addresses if requested
        if (allocFlags.allocPhys) {
            for (uint64_t i = 0; i < count; i++) {
                Anon* anon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                anon->refCount = 1;
                anon->physAddr = (uint64_t)g_PMM->AllocatePage();
                if (allocFlags.zero)
                    memset((void*)to_HHDM(anon->physAddr), 0, PAGE_SIZE);
                m_pageMapper->MapPage(((uint64_t)pages + i * PAGE_SIZE), anon->physAddr, allocFlags.protection, allocFlags.user, allocFlags.cacheType);
                map->slots[i] = anon;
            }
        }

        // Step 4: Build the map entry
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = (uint64_t)pages + count * PAGE_SIZE;
        entry->anonMap = map;
        entry->flags.protection = allocFlags.protection;
        entry->flags.cacheType = allocFlags.cacheType;
        entry->flags.user = allocFlags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = allocFlags.isPrivate;
        entry->flags.zero = allocFlags.zero;

        m_mapEntries.lock();
        m_mapEntries.Insert((uint64_t)pages, entry);
        m_mapEntries.unlock();

        if (allocFlags.allocPhys)
            m_pageMapper->InvalidatePages((uint64_t)pages, count);

        return pages;
    }

    void* VMM::AllocateBackedPages(uint64_t count, MemoryObject* obj, uint64_t offset, void* addr, AllocFlags allocFlags) {
        if (count == 0 || obj == nullptr)
            return nullptr;

        uint64_t page = offset >> PAGE_SIZE_SHIFT;

        spinlock_acquire(&obj->lock);
        if ((page + count) > obj->size) {
            spinlock_release(&obj->lock);
            return nullptr;
        }

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
                    m_mapEntries.lock();
                    m_vmRegionAllocator->Lock();
                    bool rc = Internal_FreePages(addr, count, true, false);
                    m_mapEntries.unlock();
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

        // Step 3: Get an AnonMap if it is a private mapping
        AnonMap* map = nullptr;

        if (allocFlags.isPrivate) {
            map = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
            if (map == nullptr) {
                kfree_vmm(entry);
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }

            map->slotCount = count;
            map->refCount = 1;
            map->slots = (Anon**)kcalloc_vmm(count, sizeof(Anon*));
            if (map->slots == nullptr) {
                kfree_vmm(map);
                kfree_vmm(entry);
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }
        }

        // Step 4: Allocate physical addresses if requested
        if (allocFlags.allocPhys) {
            for (uint64_t i = 0; i < count; i++) {
                bool write = ((uint8_t)allocFlags.protection & (uint8_t)Protection::WRITE) > 0;
                Page* page;
                bool rc = obj->pager->GetPage(obj, offset + i * PAGE_SIZE, &page, write);
                if (!rc) {
                    assert(false);
                }
                if (allocFlags.isPrivate && write) {
                    Anon* anon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                    anon->refCount = 1;
                    anon->physAddr = (uint64_t)g_PMM->AllocatePage();
                    memcpy(to_HHDM((void*)anon->physAddr), to_HHDM((void*)page->physAddr), PAGE_SIZE);
                    m_pageMapper->MapPage(((uint64_t)pages + i * PAGE_SIZE), anon->physAddr, allocFlags.protection, allocFlags.user, allocFlags.cacheType);
                    map->slots[i] = anon;
                } else
                    m_pageMapper->MapPage(((uint64_t)pages + i * PAGE_SIZE), page->physAddr, allocFlags.protection, allocFlags.user, allocFlags.cacheType);
            }
        }

        // Step 5: Build the MapEntry
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = entry->startVirt + count * PAGE_SIZE;
        entry->anonMap = map;
        entry->memoryObject = obj;
        entry->offset = offset;
        entry->flags.protection = allocFlags.protection;
        entry->flags.cacheType = allocFlags.cacheType;
        entry->flags.user = allocFlags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = allocFlags.isPrivate;
        entry->flags.zero = false;

        obj->refCount++;
        spinlock_release(&obj->lock);

        m_mapEntries.lock();
        m_mapEntries.Insert((uint64_t)pages, entry);
        m_mapEntries.unlock();

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
        MemoryObject* obj = nullptr;
        bool lockedObj = false;
        if (objOut != nullptr)
            obj = *objOut;
        if (obj == nullptr) {
            obj = (MemoryObject*)kcalloc_vmm(1, sizeof(MemoryObject));
            if (obj == nullptr) {
                m_vmRegionAllocator->FreePages(pages, count);
                return nullptr;
            }

            obj->pager = pager;
            obj->pagerData = pagerData;
            obj->size = count;
            obj->refCount = 1;
        } else {
            spinlock_acquire(&obj->lock);
            obj->size += count;
            obj->refCount++;
        }

        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry)); // allocate this now for easier error handling
        if (entry == nullptr) {
            kfree_vmm(obj);
            m_vmRegionAllocator->FreePages(pages, count);
            return nullptr;
        }

        // Step 3: build the page list
        if (flags.allocPhys) {
            for (uint64_t i = 0; i < count; i++) {
                Page* page = (Page*)kcalloc_vmm(1, sizeof(Page));
                page->protection = flags.protection;
                page->isWired = flags.allocPhys;
                page->physAddr = (uint64_t)g_PMM->AllocatePage();
                if (flags.zero)
                    memset((void*)to_HHDM(page->physAddr), 0, PAGE_SIZE);
                m_pageMapper->MapPage((uint64_t)pages + i * PAGE_SIZE, page->physAddr, flags.protection, flags.user, flags.cacheType);
                obj->pages.Insert(offset + i * PAGE_SIZE, page);
            }
        }

        // Step 4: build the map entry
        entry->memoryObject = obj;
        entry->startVirt = (uint64_t)pages;
        entry->endVirt = (uint64_t)pages + count * PAGE_SIZE;
        entry->wireCount = flags.allocPhys ? count : 0;
        entry->offset = offset;
        entry->flags.protection = flags.protection;
        entry->flags.cacheType = flags.cacheType;
        entry->flags.user = flags.user;
        entry->flags.needsCopy = false;
        entry->flags.isPrivate = flags.isPrivate;
        entry->flags.zero = flags.zero;

        m_mapEntries.lock();
        m_mapEntries.Insert((uint64_t)pages, entry);
        m_mapEntries.unlock();

        if (lockedObj)
            spinlock_release(&obj->lock);

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
            m_mapEntries.lock();
            AVLTree::wAVLTreeNode* node = nullptr;
            if (full)
                node = m_mapEntries.FindNode(virt);
            else
                node = m_mapEntries.FindNodeOrLower(virt);
            if (node == nullptr || node->value == 0) {
                m_mapEntries.unlock();
                return false;
            }

            uint64_t count = 1;
            if (first && !multipleRegions)
                count = totalCount;

            MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
            if ((full && entry->startVirt != virt) || (!full && entry->endVirt < virt + count * PAGE_SIZE)) {
                m_mapEntries.unlock();
                return false;
            }

            if (!first && multipleRegions)
                count = MIN((entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT, totalCount - currentCount);

            if (!full && (entry->startVirt != virt || entry->endVirt != virt + count * PAGE_SIZE)) {
                uint64_t start = entry->startVirt;
                uint64_t end = entry->endVirt;
                if (entry->startVirt < virt) {
                    MapEntry* newEntry = SplitMapEntry(entry, (virt - entry->startVirt) >> PAGE_SIZE_SHIFT);
                    if (newEntry == nullptr) {
                        m_mapEntries.unlock();
                        return false;
                    }
                    m_mapEntries.Insert(newEntry->startVirt, newEntry);
                    entry = newEntry;
                }
                if (entry->endVirt > virt + count * PAGE_SIZE) {
                    MapEntry* newEntry = SplitMapEntry(entry, count);
                    if (newEntry == nullptr) {
                        m_mapEntries.unlock();
                        return false;
                    }
                    m_mapEntries.Insert(newEntry->startVirt, newEntry);
                }
                if (!m_vmRegionAllocator->ResizeAllocatedRegion((void*)start, (end - start) >> PAGE_SIZE_SHIFT, virtAddr, count)) {
                    m_mapEntries.unlock();
                    return false;
                }
            } else if (full)
                count = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;

            AnonMap* map = entry->anonMap;
            MemoryObject* obj = entry->memoryObject;

            if (map != nullptr) {
                spinlock_acquire(&map->lock);

                if (obj != nullptr) {
                    spinlock_acquire(&obj->lock);
                    struct Data {
                        Protection prot;
                        bool valid;
                    } data = {prot, true};
                    obj->pages.forEach([](void* data, uint64_t addr, Page* page) -> bool {
                        Data* d = static_cast<Data*>(data);
                        if (!isLessOrEqualProt(d->prot, page->protection)) {
                            d->valid = false;
                            return false;
                        }
                        return true;
                    }, &data, entry->offset);
                    if (!data.valid) {
                        spinlock_release(&obj->lock);
                        spinlock_release(&map->lock);
                        m_mapEntries.unlock();
                        return false;
                    }
                }
                
                // Now that it is confirmed to be valid, we can remap
                for (uint64_t i = 0; i < count; i++) {
                    Anon* anon = map->slots[i];
                    if (anon != nullptr) {
                        m_pageMapper->RemapPage(virt + i * PAGE_SIZE, prot, user, cacheType);
                    } else if (obj != nullptr) {
                        Page* page = obj->pages.Find(entry->offset + i);
                        if (page != nullptr)
                            m_pageMapper->RemapPage(virt + i * PAGE_SIZE, prot, user, cacheType);
                    }
                }

                if (obj != nullptr)
                    spinlock_release(&obj->lock);

                spinlock_release(&entry->anonMap->lock);
            } else if (obj != nullptr) {
                spinlock_acquire(&obj->lock);
                struct Data {
                    Protection prot;
                    bool valid;
                } data = {prot, true};
                obj->pages.forEach([](void* data, uint64_t offset, Page* page) -> bool {
                    Data* d = static_cast<Data*>(data);
                    if (!isLessOrEqualProt(d->prot, page->protection)) {
                        d->valid = false;
                        return false;
                    }
                    return true;
                }, &data, entry->offset);
                if (!data.valid) {
                    spinlock_release(&obj->lock);
                    spinlock_release(&map->lock);
                    m_mapEntries.unlock();
                    return false;
                }

                struct RemapData {
                    uint64_t virt;
                    uint64_t entryOffset;
                    Protection prot;
                    bool user;
                    CacheType cacheType;
                    PageMapper* pageMapper;
                } remapData = {virt, entry->offset, prot, user, cacheType, m_pageMapper};
                obj->pages.forEach([](void* data, uint64_t offset, Page* page) -> void {
                    RemapData* d = static_cast<RemapData*>(data);
                    d->pageMapper->RemapPage((uint64_t)d->virt + (offset - d->entryOffset) * PAGE_SIZE, d->prot, d->user, d->cacheType);
                }, &remapData);

                spinlock_release(&obj->lock);
            }

            Protection oldProt = entry->flags.protection;
            bool wasUser = entry->flags.user;

            entry->flags.protection = prot;
            entry->flags.user = user;

            m_mapEntries.unlock(); // need to hold the lock for the whole function to ensure it can't be unmapped on us part way through

            shootdown |= (user ^ wasUser ) || m_pageMapper->isPermsReduction(oldProt, prot);

            currentCount += count;
            first = false;
            virt += count * PAGE_SIZE;
        }

        m_pageMapper->InvalidatePages((uint64_t)virtAddr, totalCount, shootdown);
        return true;
    }

    bool VMM::MapPages(void* virtAddr, uint64_t count) {
        m_mapEntries.lock();

        AVLTree::wAVLTreeNode* node = m_mapEntries.FindNodeOrLower((uint64_t)virtAddr);
        if (node == nullptr || node->value == 0) {
            m_mapEntries.unlock();
            return false;
        }

        MapEntry* entry = (MapEntry*)node->value;
        if (entry->endVirt <= (uint64_t)virtAddr || ((uint64_t)virtAddr + count * PAGE_SIZE) > entry->endVirt || entry->anonMap == nullptr) {
            m_mapEntries.unlock();
            return false;
        }
        AnonMap* map = entry->anonMap;

        spinlock_acquire(&map->lock);

        for (uint64_t i = ((uint64_t)virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT; i < map->slotCount; i++) {
            Anon* anon = map->slots[i];
            if (anon == nullptr) {
                anon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                anon->refCount = 1;
                anon->physAddr = (uint64_t)g_PMM->AllocatePage();
                m_pageMapper->MapPage(entry->startVirt + i * PAGE_SIZE, anon->physAddr, entry->flags.protection, entry->flags.user, entry->flags.cacheType);
                map->slots[i] = anon;
            }
        }

        spinlock_release(&map->lock);
        
        m_mapEntries.unlock();
        
        m_pageMapper->InvalidatePages((uint64_t)virtAddr, count, false); // Not a permission reduction, shootdown is not required
        return true;
    }

    bool VMM::HandlePageFault(PageFaultCode code, uint64_t virtAddr) {
        if (m_vmRegionAllocator == nullptr || virtAddr < m_vmRegionAllocator->GetStart() || virtAddr >= m_vmRegionAllocator->GetEnd())
            return false; // outside the region

        m_mapEntries.lock();
        AVLTree::wAVLTreeNode* node = m_mapEntries.FindNodeOrLower(virtAddr);
        if (node == nullptr || node->value == 0) {
            m_mapEntries.unlock();
            return false;
        }

        MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
        if (virtAddr < entry->startVirt || virtAddr >= entry->endVirt || (entry->anonMap == nullptr && entry->memoryObject == nullptr)) {
            m_mapEntries.unlock();
            return false;
        }

        AnonMap* map = entry->anonMap;
        MemoryObject* obj = entry->memoryObject;
        virtAddr = ALIGN_DOWN(virtAddr, PAGE_SIZE);
        uint64_t pageIndex = (virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT;
        Protection prot = entry->flags.protection;
        CacheType cacheType = entry->flags.cacheType;
        bool user = entry->flags.user;
        bool zero = entry->flags.zero;
        bool copy = entry->flags.needsCopy;
        uint64_t offset = entry->offset;

        // need to validate that the page fault was actually caused by a mismatch in protection
        bool valid = false;
        switch (prot) {
        case Protection::READ:
            valid = !code.write && !code.execute;
            break;
        case Protection::WRITE:
            valid = code.write && !code.execute;
            break;
        case Protection::EXECUTE:
            valid = code.execute; // no way to differentiate between read and write, so don't bother
            break;
        case Protection::READ_WRITE:
            valid = !code.execute;
            break;
        case Protection::READ_EXECUTE:
            valid = !code.write;
            break;
        case Protection::READ_WRITE_EXECUTE:
            valid = true;
            break;
        }
        if (!valid) {
            m_mapEntries.unlock();
            return false;
        }
        
        if (map != nullptr) {
            spinlock_acquire(&map->lock);
            m_mapEntries.unlock();

            Anon* anon = map->slots[pageIndex];

            if (anon != nullptr) { // not mapped here, but is somewhere else
                bool result = true;

                bool isShared = anon->refCount > 1;

                if (code.write && isShared) {
                    Anon* newAnon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                    if (newAnon == nullptr) {
                        spinlock_release(&map->lock);
                        return false;
                    }

                    newAnon->refCount = 1;
                    newAnon->physAddr = reinterpret_cast<uint64_t>(g_PMM->AllocatePage());
                    if (newAnon->physAddr == 0) {
                        kfree_vmm(newAnon);
                        spinlock_release(&map->lock);
                        return false;
                    }

                    if (obj == nullptr)
                        memcpy(to_HHDM((void*)newAnon->physAddr), to_HHDM((void*)anon->physAddr), PAGE_SIZE);
                    else {
                        Page* page = nullptr;
                        spinlock_acquire(&obj->lock);
                        result = obj->pager->GetPage(obj, offset + pageIndex * PAGE_SIZE, &page, code.write);
                        if (result)
                            memcpy(to_HHDM((void*)newAnon->physAddr), to_HHDM((void*)page->physAddr), PAGE_SIZE);
                        spinlock_release(&obj->lock);
                    }
                    if (result)
                        result = m_pageMapper->MapPage(virtAddr, newAnon->physAddr, prot, user, cacheType);

                    if (result) {
                        map->slots[pageIndex] = newAnon;
                        anon->refCount--;
                        if (anon->refCount == 0) {
                            g_PMM->FreePage((void*)anon->physAddr);
                            kfree_vmm(anon);
                        }
                    } else {
                        g_PMM->FreePage((void*)newAnon->physAddr);
                        kfree_vmm(newAnon);
                    }

                    spinlock_release(&map->lock);
                    return result;
                } else {
                    // If isShared is false, we naturally restore Read-Write access without an unnecessary copy!
                    Protection mapProt = prot;
                    if (isShared) {
                        mapProt = static_cast<Protection>(static_cast<uint8_t>(prot) & ~static_cast<uint8_t>(Protection::WRITE));
                    }

                    if (code.present)
                        result = m_pageMapper->RemapPage(virtAddr, mapProt, user, cacheType);
                    else
                        result = m_pageMapper->MapPage(virtAddr, anon->physAddr, mapProt, user, cacheType);
                }
                spinlock_release(&map->lock);
                return result;
            }

            bool result = false;
            if (obj != nullptr) {
                Page* page = nullptr;
                spinlock_acquire(&obj->lock);
                bool rc = obj->pager->GetPage(obj, offset + pageIndex * PAGE_SIZE, &page, code.write);
                if (!rc) {
                    spinlock_release(&obj->lock);
                    return false;
                }

                if (code.write && copy) {
                    if (map == nullptr) {
                        map = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
                        if (map == nullptr) {
                            spinlock_release(&obj->lock);
                            return false;
                        }

                        map->slotCount = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;
                        map->refCount = 1;
                        map->slots = (Anon**)kcalloc_vmm(map->slotCount, sizeof(Anon*));
                        if (map->slots == nullptr) {
                            kfree_vmm(map);
                            spinlock_release(&obj->lock);
                            return false;
                        }

                        entry->anonMap = map;
                    }

                    Anon* newAnon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                    if (newAnon == nullptr) {
                        spinlock_release(&obj->lock);
                        return false;
                    }

                    newAnon->refCount = 1;
                    newAnon->physAddr = reinterpret_cast<uint64_t>(g_PMM->AllocatePage());
                    if (newAnon->physAddr == 0) {
                        kfree_vmm(newAnon);
                        spinlock_release(&obj->lock);
                        return false;
                    }

                    memcpy(to_HHDM((void*)newAnon->physAddr), to_HHDM((void*)page->physAddr), PAGE_SIZE);
                    result = m_pageMapper->MapPage(virtAddr, newAnon->physAddr, prot, user, cacheType);
                    if (result)
                        map->slots[pageIndex] = newAnon;
                    else {
                        g_PMM->FreePage((void*)newAnon->physAddr);
                        kfree_vmm(newAnon);
                    }

                    spinlock_release(&obj->lock);
                    return result;
                }

                if (copy) // map as read-only if this is not a write fault, and it would need to be copied
                    prot = (Protection)((uint8_t)prot & ~(uint8_t)Protection::WRITE);
                result = m_pageMapper->MapPage(virtAddr, page->physAddr, prot, user, cacheType);
                spinlock_release(&obj->lock);
            } else {
                if (!code.present) {
                    anon = (Anon*)kcalloc_vmm(1, sizeof(Anon));
                    if (anon == nullptr) {
                        spinlock_release(&map->lock);
                        return false;
                    }

                    anon->refCount = 1;
                    anon->physAddr = reinterpret_cast<uint64_t>(g_PMM->AllocatePage());
                    map->slots[pageIndex] = anon;
                    if (zero)
                        memset(reinterpret_cast<void*>(to_HHDM(anon->physAddr)), 0, PAGE_SIZE);
                    result = m_pageMapper->MapPage(virtAddr, anon->physAddr, prot, user, cacheType);
                }
            }

            spinlock_release(&map->lock);
            return result;
        } else {
            spinlock_acquire(&obj->lock);
            m_mapEntries.unlock();

            Page* page = nullptr;
            bool rc = obj->pager->GetPage(obj, offset + pageIndex * PAGE_SIZE, &page, code.write);
            if (!rc || page == nullptr) {
                spinlock_release(&obj->lock);
                return false;
            }

            if (page->physAddr != 0) { // not mapped here, but it has a physical address
                bool result;
                if (code.present)
                    result = m_pageMapper->RemapPage(virtAddr, prot, user, cacheType);
                else
                    result = m_pageMapper->MapPage(virtAddr, page->physAddr, prot, user, cacheType);
                spinlock_release(&obj->lock);
                return result;
            }

            bool result = false;
            if (!code.present) {
                page->physAddr = reinterpret_cast<uint64_t>(g_PMM->AllocatePage());
                if (zero)
                    memset(reinterpret_cast<void*>(to_HHDM(page->physAddr)), 0, PAGE_SIZE);
                result = m_pageMapper->MapPage(virtAddr, page->physAddr, prot, user, cacheType);
            }

            spinlock_release(&obj->lock);
            return result;
        }
    }

    bool VMM::ValidateRead(const void* addr, size_t size, bool user) {
        uint64_t virtAddr = (uint64_t)addr;

        m_mapEntries.lock();

        while (true) {
            AVLTree::wAVLTreeNode* node = m_mapEntries.FindNodeOrLower(virtAddr);
            if (node == nullptr || node->value == 0) {
                m_mapEntries.unlock();
                return false;
            }

            MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
            Protection prot = entry->flags.protection;
            if (virtAddr < entry->startVirt || (user && !entry->flags.user) || (static_cast<uint8_t>(prot) & static_cast<uint8_t>(Protection::READ)) == 0) {
                m_mapEntries.unlock();
                return false;
            }

            if ((virtAddr + size) <= entry->endVirt) {
                m_mapEntries.unlock();
                return true;
            }

            size -= entry->endVirt - virtAddr;
            virtAddr = entry->endVirt;
        }
    }

    bool VMM::ValidateWrite(const void* addr, size_t size, bool user) {
        uint64_t virtAddr = (uint64_t)addr;

        m_mapEntries.lock();

        while (true) {
            AVLTree::wAVLTreeNode* node = m_mapEntries.FindNodeOrLower(virtAddr);
            if (node == nullptr || node->value == 0) {
                m_mapEntries.unlock();
                return false;
            }

            MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
            Protection prot = entry->flags.protection;
            if (virtAddr < entry->startVirt || (user && !entry->flags.user) || (static_cast<uint8_t>(prot) & static_cast<uint8_t>(Protection::WRITE)) == 0) {
                m_mapEntries.unlock();
                return false;
            }

            if ((virtAddr + size) <= entry->endVirt) {
                m_mapEntries.unlock();
                return true;
            }

            size -= entry->endVirt - virtAddr;
            virtAddr = entry->endVirt;
        }
    }

    bool VMM::CopyStringFromUser(char* kernelDst, const void* userSrc, size_t maxSize, size_t* outSize, bool user) {
        uint64_t virtAddr = reinterpret_cast<uint64_t>(userSrc);
        size_t totalCopied = 0;

        if (maxSize == 0) {
            if (outSize != nullptr)
                *outSize = 0;
            return false;
        }

        m_mapEntries.lock();

        while (totalCopied < maxSize) {
            AVLTree::wAVLTreeNode* node = m_mapEntries.FindNodeOrLower(virtAddr);
            if (node == nullptr || node->value == 0) {
                m_mapEntries.unlock();
                if (outSize != nullptr)
                    *outSize = totalCopied;
                return false; // Unmapped memory hit
            }

            MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
            Protection prot = entry->flags.protection;

            // Verify bounds and READ permissions
            if (virtAddr < entry->startVirt || virtAddr >= entry->endVirt || 
            (user && !entry->flags.user) || 
            (static_cast<uint8_t>(prot) & static_cast<uint8_t>(Protection::READ)) == 0) {
                m_mapEntries.unlock();
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
            if (entry->anonMap != nullptr) {
                spinlock_acquire(&entry->anonMap->lock);
                Anon* anon = entry->anonMap->slots[pageIndex];
                if (anon != nullptr && anon->physAddr != 0)
                    srcHHDM = reinterpret_cast<const char*>(to_HHDM(anon->physAddr)) + pageOffset;
                else if (entry->flags.zero)
                    isZeroPage = true;
                spinlock_release(&entry->anonMap->lock);
            } else if (entry->memoryObject != nullptr) {
                MemoryObject* obj = entry->memoryObject;
                spinlock_acquire(&obj->lock);
                Page* page = obj->pages.Find(entry->offset + pageIndex * PAGE_SIZE);
                if (page != nullptr && page->physAddr != 0)
                    srcHHDM = reinterpret_cast<const char*>(to_HHDM(page->physAddr)) + pageOffset;
                else if (entry->flags.zero)
                    isZeroPage = true;
                spinlock_release(&obj->lock);
            }

            // Step 2: Handle unpopulated file-backed pages safely
            if (srcHHDM == nullptr && !isZeroPage) {
                // Memory is mapped but frame isn't in RAM yet (e.g., pager needs to load it).
                // Unlock m_mapEntries, trigger Page Fault to bring frame into RAM, then retry.
                m_mapEntries.unlock();
                PageFaultCode code = { .present = false, .write = false, .user = user, .execute = false };
                if (!HandlePageFault(code, virtAddr)) {
                    if (outSize != nullptr)
                        *outSize = totalCopied;
                    return false; // Pager failed to load page
                }
                m_mapEntries.lock();
                continue; // Retry this address now that it's paged in
            }

            // Step 3: Copy & scan bytes
            if (isZeroPage) {
                // Demand-zero page: first byte is '\0'
                kernelDst[totalCopied] = '\0';
                m_mapEntries.unlock();
                if (outSize != nullptr)
                    *outSize = totalCopied + 1;
                return true; // Successfully found null terminator!
            }

            for (size_t i = 0; i < bytesToProcess; ++i) {
                char c = srcHHDM[i];
                kernelDst[totalCopied] = c;
                totalCopied++;

                if (c == '\0') {
                    m_mapEntries.unlock();
                    if (outSize != nullptr)
                        *outSize = totalCopied;
                    return true; // Null terminator reached
                }
            }

            virtAddr += bytesToProcess;
        }

        m_mapEntries.unlock();

        // Reached maxSize without seeing '\0'
        if (outSize != nullptr)
            *outSize = totalCopied;
        return false;
    }

    bool VMM::Fork(VMM* other) {
        other->m_mapEntries.lock();
        m_mapEntries.lock();
        struct Data {
            VMM* current;
            VMM* other;
            bool success;
        } data = {this, other, true};
        other->m_mapEntries.forEach([](void* data, uint64_t virt, MapEntry* entry) -> bool {
            Data* d = static_cast<Data*>(data);
            MapEntry* newEntry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry));
            if (newEntry == nullptr) {
                d->success = false;
                return false;
            }

            if (entry->anonMap != nullptr) {
                spinlock_acquire(&entry->anonMap->lock);
                AnonMap* map = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
                Anon** slots = (Anon**)kcalloc_vmm(entry->anonMap->slotCount, sizeof(Anon*));
                if (map == nullptr || slots == nullptr) {
                    if (map != nullptr)
                        kfree_vmm(map);
                    if (slots != nullptr)
                        kfree_vmm(slots);
                    kfree_vmm(newEntry);
                    spinlock_release(&entry->anonMap->lock);
                    d->success = false;
                    return false;
                }

                memcpy(slots, entry->anonMap->slots, entry->anonMap->slotCount * sizeof(Anon*));

                for (uint64_t i = 0; i < entry->anonMap->slotCount; i++) {
                    if (slots[i] != nullptr)
                        slots[i]->refCount++;
                }

                map->slotCount = entry->anonMap->slotCount;
                map->slots = slots;
                map->refCount = 1;

                spinlock_release(&entry->anonMap->lock);

                newEntry->anonMap = map;
            }

            if (entry->memoryObject != nullptr) {
                spinlock_acquire(&entry->memoryObject->lock);
                newEntry->memoryObject = entry->memoryObject;
                newEntry->memoryObject->refCount++;
                spinlock_release(&entry->memoryObject->lock);
            }

            newEntry->startVirt = entry->startVirt;
            newEntry->endVirt = entry->endVirt;
            newEntry->offset = entry->offset;

            entry->flags.needsCopy = entry->flags.isPrivate; // needs to be set on both, only for private mappings
            newEntry->flags = entry->flags;

            // Downgrade the parent's page tables to Read-Only so it catches COW faults
            if (entry->flags.isPrivate && (static_cast<uint8_t>(entry->flags.protection) & static_cast<uint8_t>(Protection::WRITE))) {
                Protection roProt = static_cast<Protection>(static_cast<uint8_t>(entry->flags.protection) & ~static_cast<uint8_t>(Protection::WRITE));
                uint64_t count = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;
                d->other->m_pageMapper->RemapPages(entry->startVirt, count, roProt, entry->flags.user, entry->flags.cacheType);
                d->other->m_pageMapper->InvalidatePages(entry->startVirt, count, true); // Flush the TLB![cite: 10]
            }

            d->current->m_mapEntries.Insert(newEntry->startVirt, newEntry);

            return true;
        }, &data);
        m_mapEntries.unlock();
        other->m_mapEntries.unlock();
        return data.success;
    }

    PageMapper* VMM::GetPageMapper() {
        return m_pageMapper;
    }

    VMRegionAllocator* VMM::GetAllocator() {
        return m_vmRegionAllocator;
    }

    void VMM::DumpRegions(fd_t fd) {
        fprintf(fd, "\nMap Entries:\n");
        m_mapEntries.lock();
        m_mapEntries.forEach([](void* data, uint64_t key, MapEntry* entry) -> void {
            fd_t fd = (fd_t)data;
            fprintf(fd, "Entry: %lx-%lx, offset = %lx, anonMap = %p, memoryObject = %p, Flags:\n\tprot = %x\n\tcacheType = %x\n\tuser = %s, needsCopy = %s, isPrivate = %s, zero = %s\n", entry->startVirt, entry->endVirt, entry->offset, entry->anonMap, entry->memoryObject, entry->flags.protection, entry->flags.cacheType, entry->flags.user ? "true" : "false", entry->flags.needsCopy ? "true" : "false", entry->flags.isPrivate ? "true" : "false", entry->flags.zero ? "true" : "false");
            if (entry->anonMap != nullptr) {
                AnonMap* map = entry->anonMap;
                spinlock_acquire(&map->lock);
                fprintf(fd, "AnonMap: refCount = %lu, slotCount = %lx\n", map->refCount, map->slotCount);
                spinlock_release(&map->lock);
            }
            if (entry->memoryObject != nullptr) {
                MemoryObject* obj = entry->memoryObject;
                spinlock_acquire(&obj->lock);
                fprintf(fd, "MemoryObject: size = %lx, refCount = %lu, pager = %p, pagerData = %p\n", obj->size, obj->refCount, obj->pager, obj->pagerData);
                spinlock_release(&obj->lock);
            }
            fputc(fd, '\n');
        }, (void*)fd);
        m_mapEntries.unlock();
        fputc(fd, '\n');
    }

    // split a map entry so that entry has a page count of newPageCount, returns the new upper part
    MapEntry* VMM::SplitMapEntry(MapEntry* entry, uint64_t newPageCount) {
        MapEntry* newEntry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry));
        if (newEntry == nullptr)
            return nullptr;
        newEntry->startVirt = entry->startVirt + newPageCount * PAGE_SIZE;
        newEntry->endVirt = entry->endVirt;

        uint64_t upperPageCount = (newEntry->endVirt - newEntry->startVirt) >> PAGE_SIZE_SHIFT;

        if (entry->anonMap != nullptr) {
            AnonMap* map = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
            if (map == nullptr) {
                kfree_vmm(newEntry);
                return nullptr;
            }
            map->slotCount = upperPageCount;
            map->refCount = 1;
            map->slots = (Anon**)kcalloc_vmm(upperPageCount, sizeof(Anon*));
            Anon** newSlots = (Anon**)kcalloc_vmm(newPageCount, sizeof(Anon*));
            if (newSlots == nullptr || map->slots == nullptr) {
                if (newSlots != nullptr)
                    kfree_vmm(newSlots);
                if (map->slots != nullptr)
                    kfree_vmm(map->slots);
                kfree_vmm(map);
                kfree_vmm(newEntry);
                return nullptr;
            }

            spinlock_acquire(&entry->anonMap->lock);
            if (entry->anonMap->refCount > 1) {
                // This AnonMap is referenced elsewhere, so we need to make a new AnonMap
                AnonMap* newMap = (AnonMap*)kcalloc_vmm(1, sizeof(AnonMap));
                if (newMap == nullptr) {
                    spinlock_release(&entry->anonMap->lock);
                    kfree_vmm(newSlots);
                    kfree_vmm(map->slots);
                    kfree_vmm(map);
                    kfree_vmm(newEntry);
                    return nullptr;
                }
                newMap->slots = newSlots;
                newMap->slotCount = newPageCount;
                newMap->refCount = 1;

                memcpy(newSlots, entry->anonMap->slots, sizeof(Anon*) * newPageCount);
                memcpy(map->slots, &entry->anonMap->slots[newPageCount], sizeof(Anon*) * upperPageCount);

                entry->anonMap->refCount--;
                spinlock_release(&entry->anonMap->lock);

                entry->anonMap = newMap;
            } else {
                memcpy(newSlots, entry->anonMap->slots, sizeof(Anon*) * newPageCount);
                memcpy(map->slots, &entry->anonMap->slots[newPageCount], sizeof(Anon*) * upperPageCount);
                kfree_vmm(entry->anonMap->slots);
                entry->anonMap->slots = newSlots;
                entry->anonMap->slotCount = newPageCount;
                spinlock_release(&entry->anonMap->lock);
            }
            
            newEntry->anonMap = map;
        }
        if (entry->memoryObject != nullptr) {
            newEntry->offset = entry->offset + newPageCount;
            newEntry->memoryObject = entry->memoryObject;
            spinlock_acquire(&newEntry->memoryObject->lock);
            newEntry->memoryObject->refCount++;
            spinlock_release(&newEntry->memoryObject->lock);
        }

        newEntry->flags = entry->flags;

        entry->endVirt = newEntry->startVirt;

        return newEntry;
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
                m_mapEntries.lock();
            AVLTree::wAVLTreeNode* node = nullptr;
            if (full)
                node = m_mapEntries.FindNode(virt);
            else
                node = m_mapEntries.FindNodeOrLower(virt);
            if (node == nullptr || node->value == 0) {
                if (lock)
                    m_mapEntries.unlock();
                return false;
            }

            uint64_t count = 1;
            if (first && !multipleRegions)
                count = totalCount;

            MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
            if ((full && entry->startVirt != virt) || (!full && entry->endVirt < virt + count * PAGE_SIZE)) {
                if (lock)
                    m_mapEntries.unlock();
                return false;
            }

            if (!first && multipleRegions)
                count = MIN((entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT, totalCount - currentCount);

            if (!full && (entry->startVirt != virt || entry->endVirt != virt + count * PAGE_SIZE)) {
                if (entry->startVirt < virt) {
                    MapEntry* newEntry = SplitMapEntry(entry, (virt - entry->startVirt) >> PAGE_SIZE_SHIFT);
                    if (newEntry == nullptr) {
                        if (lock)
                            m_mapEntries.unlock();
                        return false;
                    }
                    entry = newEntry;
                } else
                    m_mapEntries.RemoveNode(node);
                if (entry->endVirt > virt + count * PAGE_SIZE) {
                    MapEntry* newEntry = SplitMapEntry(entry, count);
                    if (newEntry == nullptr) {
                        if (lock)
                            m_mapEntries.unlock();
                        return false;
                    }
                    m_mapEntries.Insert(newEntry->startVirt, newEntry);
                }
            } else {
                if (full)
                    count = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;
                m_mapEntries.RemoveNode(node);
            }

            if (lock)
                m_mapEntries.unlock();

            m_vmRegionAllocator->FreePages(reinterpret_cast<void*>(virt), count, full, lock);

            
            if (entry->anonMap != nullptr) {
                AnonMap* map = entry->anonMap;
                spinlock_acquire(&map->lock);
                map->refCount--;
                
                MemoryObject* obj = entry->memoryObject;
                if (obj != nullptr)
                    spinlock_acquire(&obj->lock);

                uint64_t lowestMapped = UINT64_MAX;
                uint64_t highestMapped = 0;

                // go through once and unmap the pages
                for (uint64_t i = 0; i < map->slotCount; i++) {
                    Anon* anon = map->slots[i];
                    if (anon != nullptr) {
                        if (lowestMapped > i)
                            lowestMapped = i;
                        highestMapped = i;
                        m_pageMapper->UnmapPage(entry->startVirt + i * PAGE_SIZE);
                    } else if (obj != nullptr) {
                        Page* page = obj->pages.Find(entry->offset + i);
                        if (page != nullptr) {
                            if (lowestMapped > i)
                                lowestMapped = i;
                            highestMapped = i;
                            m_pageMapper->UnmapPage(entry->startVirt + i * PAGE_SIZE);
                        }
                    }
                }

                // Invalidate the unmapped pages
                if (lowestMapped != UINT64_MAX)
                    m_pageMapper->InvalidatePages(reinterpret_cast<uint64_t>(virtAddr) + lowestMapped * PAGE_SIZE, (highestMapped - lowestMapped) * PAGE_SIZE, true);

                // go through a second time and free the underlying pages and structures
                for (uint64_t i = 0; i < map->slotCount; i++) {
                    Anon* anon = map->slots[i];
                    if (anon != nullptr) {
                        map->slots[i] = nullptr;
                        g_PMM->FreePage(reinterpret_cast<void*>(anon->physAddr));
                        anon->refCount--;
                        if (anon->refCount == 0)
                            kfree_vmm(anon);
                    }
                }

                if (map->refCount == 0)
                    kfree_vmm(map);
                else
                    spinlock_release(&map->lock);

                if (obj != nullptr) {
                    obj->refCount--;
                    if (obj->refCount == 0) {
                        obj->pages.forEach([](void*, uint64_t addr, Page* page) -> bool {
                            if (page->physAddr != 0)
                                g_PMM->FreePage(reinterpret_cast<void*>(page->physAddr));
                            kfree_vmm(page);
                            return true;
                        }, nullptr, entry->offset);

                        kfree_vmm(obj);
                    } else
                        spinlock_release(&obj->lock);
                }
            } else if (entry->memoryObject != nullptr) {
                // must be a pure memory object entry
                MemoryObject* obj = entry->memoryObject;
                spinlock_acquire(&obj->lock);
                obj->refCount--;

                struct Data {
                    PageMapper* mapper;
                    MapEntry* entry;
                    uint64_t lowest;
                    uint64_t highest;
                } data = {m_pageMapper, entry, UINT64_MAX, 0};

                // go through once and unmap the pages
                obj->pages.forEach([](void* data, uint64_t addr, Page* page) -> bool {
                    Data* d = (Data*)data;
                    if (page->physAddr == 0)
                        return true;
                    d->mapper->UnmapPage(addr);
                    if (d->lowest > addr)
                        d->lowest = addr;
                    d->highest = addr;
                    return true;
                }, &data, entry->offset);

                // Invalidate the unmapped pages
                if (data.lowest != UINT64_MAX)
                    m_pageMapper->InvalidatePages(data.lowest, data.highest - data.lowest, true);

                // go through a second time and free the underlying pages and structures
                if (obj->refCount == 0) {
                    obj->pages.forEach([](void*, uint64_t addr, Page* page) -> bool {
                        if (page->physAddr != 0)
                            g_PMM->FreePage(reinterpret_cast<void*>(page->physAddr));
                        kfree_vmm(page);
                        return true;
                    }, nullptr, entry->offset);

                    kfree_vmm(obj);
                } else
                    spinlock_release(&obj->lock);
            }

            // If somehow both the map and memory object are null, just delete the entry anyway

            kfree_vmm(entry);

            first = false;
            currentCount += count;
            virt += count * PAGE_SIZE;
        }

        return true;
    }


}