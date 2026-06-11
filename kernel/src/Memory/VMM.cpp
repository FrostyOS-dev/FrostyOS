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
            if (map != nullptr) {
                spinlock_acquire(&map->lock);
                for (uint64_t i = 0; i < map->slotCount; i++) {
                    Anon* anon = map->slots[i + entry->offset];
                    if (anon->physAddr != 0) {
                        current->m_pageMapper->UnmapPage(entry->startVirt + i * PAGE_SIZE);
                        g_PMM->FreePage((void*)anon->physAddr);
                    }
                    anon->refCount--;
                    if (anon->refCount == 0)
                        kfree_vmm(anon);
                }
                kfree_vmm(map);
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
        else
            pages = m_vmRegionAllocator->AllocatePages(addr, count);
        if (pages == nullptr)
            return nullptr;

        // Step 2: put together the memory object
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

    bool VMM::FreePages(void* virtAddr, uint64_t count) {
        uint64_t virt = reinterpret_cast<uint64_t>(virtAddr);

        bool full = false;
        if (count == 0) {
            full = true;
            count = 1;
        }

        if (m_vmRegionAllocator == nullptr || virt < m_vmRegionAllocator->GetStart() || (virt + count * PAGE_SIZE) > m_vmRegionAllocator->GetEnd())
            return false; // outside the region

        m_mapEntries.lock();
        AVLTree::wAVLTreeNode* node = m_mapEntries.FindNode(virt);
        if (node == nullptr || node->value == 0) {
            m_mapEntries.unlock();
            return false;
        }

        MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
        if (virt != entry->startVirt || entry->anonMap == nullptr || (!full && (virt + count * PAGE_SIZE) != entry->endVirt)) {
            m_mapEntries.unlock();
            return false;
        }

        m_mapEntries.RemoveNode(node);
        m_mapEntries.unlock();

        AnonMap* map = entry->anonMap;
        spinlock_acquire(&map->lock);
        map->refCount--;

        kfree_vmm(entry);

        uint64_t lowestMapped = UINT64_MAX;
        uint64_t highestMapped = 0;

        // go through once and unmap the pages
        for (uint64_t i = 0; i < map->slotCount; i++) {
            Anon* anon = map->slots[i];
            if (anon != nullptr)
                m_pageMapper->UnmapPage(entry->startVirt + i * PAGE_SIZE);
        }

        // Invalidate the unmapped pages
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

        return true;
    }

    bool VMM::RemapPages(void* virtAddr, uint64_t count, Protection prot, bool user, CacheType cacheType) {
        m_mapEntries.lock();

        bool full = count == 0;

        MapEntry* entry = m_mapEntries.Find((uint64_t)virtAddr);
        if (entry == nullptr || entry->anonMap == nullptr || (!full && (uint64_t)virtAddr + count * PAGE_SIZE > entry->endVirt)) {
            m_mapEntries.unlock();
            return false;
        }

        uint64_t pageCount = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;

        AnonMap* map = entry->anonMap;

        spinlock_acquire(&map->lock);
        
        // Now that it is confirmed to be valid, we can remap
        for (uint64_t i = 0; i < pageCount; i++) {
            Anon* anon = map->slots[i];
            if (anon != nullptr)
                m_pageMapper->RemapPage((uint64_t)virtAddr + i * PAGE_SIZE, prot, user, cacheType);
        }

        spinlock_release(&entry->anonMap->lock);

        Protection oldProt = entry->flags.protection;
        bool wasUser = entry->flags.user;

        entry->flags.protection = prot;
        entry->flags.user = user;

        m_mapEntries.unlock(); // need to hold the lock for the whole function to ensure it can't be unmapped on us part way through

        m_pageMapper->InvalidatePages((uint64_t)virtAddr, pageCount, (user ^ wasUser ) || m_pageMapper->isPermsReduction(oldProt, prot));
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
        if (virtAddr < entry->startVirt || virtAddr >= entry->endVirt || entry->anonMap == nullptr) {
            m_mapEntries.unlock();
            return false;
        }

        virtAddr = ALIGN_DOWN(virtAddr, PAGE_SIZE);
        AnonMap* map = entry->anonMap;
        uint64_t pageIndex = (virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT;
        Protection prot = entry->flags.protection;
        CacheType cacheType = entry->flags.cacheType;
        bool user = entry->flags.user;
        bool zero = entry->flags.zero;

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
        
        spinlock_acquire(&map->lock);
        m_mapEntries.unlock();

        Anon* anon = map->slots[pageIndex];

        if (anon != nullptr) { // not mapped here, but is somewhere else
            bool result;
            if (code.present)
                result = m_pageMapper->RemapPage(virtAddr, prot, user, cacheType);
            else
                result = m_pageMapper->MapPage(virtAddr, anon->physAddr, prot, user, cacheType);
            spinlock_release(&map->lock);
            return result;
        }

        
        bool result = false;
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

        spinlock_release(&map->lock);

        return result;
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


    PageMapper* VMM::GetPageMapper() {
        return m_pageMapper;
    }

    VMRegionAllocator* VMM::GetAllocator() {
        return m_vmRegionAllocator;
    }


}