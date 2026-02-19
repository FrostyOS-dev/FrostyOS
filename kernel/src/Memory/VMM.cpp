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

#include "PageMapper.hpp"
#include "Pager.hpp"
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

    void* VMM::AllocatePages(uint64_t count, Protection prot, bool allocPhys, CacheType cacheType) {
        if (count == 0 || g_defaultPager == nullptr)
            return nullptr;

        // Step 1: get a VM region. doing this first as it is more likely to fail (whilst still being quite unlikely), and easier to cleanup
        void* pages = m_vmRegionAllocator->AllocatePages(count);

        // Step 2: put together the memory object, including page list
        MemoryObject* memObj = (MemoryObject*)kcalloc_vmm(1, sizeof(MemoryObject));
        memObj->size = count * PAGE_SIZE;
        memObj->pager = g_defaultPager;

        // allocate first one outside loop to save a check on each loop iteration
        memObj->pages = (Page*)kcalloc_vmm(1, sizeof(Page));
        memObj->pages->protection = Protection::READ_WRITE_EXECUTE;
        if (allocPhys)
            memObj->pages->physAddr = reinterpret_cast<uint64_t>(memObj->pager->AllocatePage());

        Page* prevPage = memObj->pages;

        for (uint64_t i = 1; i < count; i++) {
            Page* page = (Page*)kcalloc_vmm(1, sizeof(Page));
            page->protection = Protection::READ_WRITE_EXECUTE;

            if (allocPhys)
                page->physAddr = reinterpret_cast<uint64_t>(memObj->pager->AllocatePage());

            prevPage->next = page;
            prevPage = page;
        }

        // Step 3: Map the memory
        if (!MapMemory(reinterpret_cast<uint64_t>(pages), memObj, prot, cacheType)) {
            m_vmRegionAllocator->FreePages(pages, count);

            Page* page = memObj->pages;
            for (uint64_t i = 0; i < count; i++) {
                Page* current = page;
                page = page->next;

                if (allocPhys)
                    memObj->pager->FreePage(reinterpret_cast<void*>(current->physAddr));

                kfree_vmm(current);
            }

            kfree_vmm(memObj);

            return nullptr;
        }

        return pages;
    }

    bool VMM::FreePages(void* virtAddr) {
        uint64_t virt = reinterpret_cast<uint64_t>(virtAddr);

        if (m_vmRegionAllocator == nullptr || virt < m_vmRegionAllocator->GetStart() || virt >= m_vmRegionAllocator->GetEnd())
            return false; // outside the region

        m_mapEntries.lock();
        AVLTree::wAVLTreeNode* node = m_mapEntries.FindNode(virt);
        if (node == nullptr || node->value == 0) {
            m_mapEntries.unlock();
            return false;
        }

        MapEntry* entry = reinterpret_cast<MapEntry*>(node->value);
        if (virt != entry->startVirt || entry->memoryObject == nullptr) {
            m_mapEntries.unlock();
            return false;
        }

        m_mapEntries.RemoveNode(node);
        m_mapEntries.unlock();

        MemoryObject* obj = entry->memoryObject;
        kfree_vmm(entry);

        spinlock_acquire(&obj->lock);

        Page* page = obj->pages;
        for (uint64_t i = 0; page != nullptr; i++) {
            if (page->physAddr != 0) {
                m_pageMapper->UnmapPage(virt + i * PAGE_SIZE);
                obj->pager->FreePage(reinterpret_cast<void*>(page->physAddr));
            }
            Page* current = page;
            page = page->next;
            kfree_vmm(current);
        }

        kfree_vmm(obj);

        return true;
    }

    bool VMM::MapMemory(uint64_t virtAddr, MemoryObject* memObj, Protection prot, CacheType cacheType) {
        if (memObj == nullptr)
            return false; // Invalid memory object

        spinlock_acquire(&memObj->lock);

        if (memObj->pages == nullptr || memObj->size == 0) {
            spinlock_release(&memObj->lock);
            return false;
        }

        MapEntry* entry = (MapEntry*)kcalloc_vmm(1, sizeof(MapEntry));
        if (entry == nullptr) {
            spinlock_release(&memObj->lock);
            return false;
        }

        uint64_t pageCount = memObj->size >> PAGE_SIZE_SHIFT;
        Page* page = memObj->pages;
        uint64_t i;
        for (i = 0; page != nullptr; i++) {
            // need to confirm the protection is at least as high as what it is requesting to be mapped as
            switch (page->protection) {
            case Protection::READ:
            case Protection::WRITE:
            case Protection::EXECUTE:
                if (prot != page->protection) {
                    spinlock_release(&memObj->lock);
                    kfree_vmm(entry);
                    return false;
                }
                break;
            case Protection::READ_WRITE:
                if (prot == Protection::EXECUTE || prot == Protection::READ_EXECUTE || prot == Protection::READ_WRITE_EXECUTE) {
                    spinlock_release(&memObj->lock);
                    kfree_vmm(entry);
                    return false;
                }
                break;
            case Protection::READ_EXECUTE:
                if (prot == Protection::WRITE || prot == Protection::READ_WRITE || prot == Protection::READ_WRITE_EXECUTE) {
                    spinlock_release(&memObj->lock);
                    kfree_vmm(entry);
                    return false;
                }
                break;
            default:
                break;
            }
            if (page->isWired)
                entry->wireCount++;

            page = page->next;
        }
        if (i != pageCount) {
            spinlock_release(&memObj->lock);
            kfree_vmm(entry);
            return false;
        }

        page = memObj->pages;
        for (i = 0; i < pageCount; i++) {
            if (page->physAddr != 0)
                m_pageMapper->MapPage(virtAddr + i * PAGE_SIZE, page->physAddr, prot, cacheType);
            page = page->next;
        }

        spinlock_release(&memObj->lock);

        entry->startVirt = virtAddr;
        entry->endVirt = virtAddr + pageCount * PAGE_SIZE;
        entry->memoryObject = memObj;
        entry->protection = prot;
        entry->cacheType = cacheType;

        m_mapEntries.lock();
        m_mapEntries.Insert(virtAddr, entry);
        m_mapEntries.unlock();

        m_pageMapper->InvalidatePages(virtAddr, pageCount);

        return true;
    }

    bool VMM::UnmapMemory(uint64_t virtAddr) {
        m_mapEntries.lock();

        MapEntry* entry = m_mapEntries.Find(virtAddr);
        if (entry == nullptr || entry->memoryObject == nullptr) {
            m_mapEntries.unlock();
            return false;
        }

        m_mapEntries.Remove(virtAddr); // maybe use the raw node instead?

        m_mapEntries.unlock();

        uint64_t pageCount = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;

        spinlock_acquire(&entry->memoryObject->lock);
        
        Page* page = entry->memoryObject->pages;
        for (uint64_t i = 0; i < pageCount; i++) {
            if (page == nullptr)
                break;

            if (page->physAddr != 0)
                m_pageMapper->UnmapPage(virtAddr + i * PAGE_SIZE);

            page = page->next;
        }
        spinlock_release(&entry->memoryObject->lock);

        kfree_vmm(entry);

        m_pageMapper->InvalidatePages(virtAddr, pageCount);

        return true;
    }

    bool VMM::RemapMemory(uint64_t virtAddr, Protection prot, CacheType cacheType) {
        m_mapEntries.lock();

        MapEntry* entry = m_mapEntries.Find(virtAddr);
        if (entry == nullptr || entry->memoryObject == nullptr) {
            m_mapEntries.unlock();
            return false;
        }

        uint64_t pageCount = (entry->endVirt - entry->startVirt) >> PAGE_SIZE_SHIFT;

        spinlock_acquire(&entry->memoryObject->lock);
        Page* page = entry->memoryObject->pages;
        for (uint64_t i = 0; i < pageCount; i++) {
            // need to confirm the protection is at least as high as what it is requesting to be mapped as
            switch (page->protection) {
            case Protection::READ:
            case Protection::WRITE:
            case Protection::EXECUTE:
                if (prot != page->protection) {
                    spinlock_release(&entry->memoryObject->lock);
                    m_mapEntries.unlock();
                    return false;
                }
                break;
            case Protection::READ_WRITE:
                if (prot == Protection::EXECUTE || prot == Protection::READ_EXECUTE || prot == Protection::READ_WRITE_EXECUTE) {
                    spinlock_release(&entry->memoryObject->lock);
                    m_mapEntries.unlock();
                    return false;
                }
                break;
            case Protection::READ_EXECUTE:
                if (prot == Protection::WRITE || prot == Protection::READ_WRITE || prot == Protection::READ_WRITE_EXECUTE) {
                    spinlock_release(&entry->memoryObject->lock);
                    m_mapEntries.unlock();
                    return false;
                }
                break;
            default:
                break;
            }

            page = page->next;
        }
        
        // Now that it is confirmed to be valid, we can remap
        page = entry->memoryObject->pages;
        for (uint64_t i = 0; i < pageCount; i++) {
            if (page == nullptr)
                break;

            if (page->physAddr != 0)
                m_pageMapper->RemapPage(virtAddr + i * PAGE_SIZE, prot, cacheType);

            page = page->next;
        }

        spinlock_release(&entry->memoryObject->lock);

        m_mapEntries.unlock(); // need to hold the lock for the whole function to ensure it can't be unmapped on us part way through

        m_pageMapper->InvalidatePages(virtAddr, pageCount);
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
        if (virtAddr < entry->startVirt || virtAddr >= entry->endVirt || entry->memoryObject == nullptr || entry->memoryObject->pager == nullptr) {
            m_mapEntries.unlock();
            return false;
        }

        virtAddr = ALIGN_DOWN(virtAddr, PAGE_SIZE);
        MemoryObject* obj = entry->memoryObject;
        uint64_t pageIndex = (virtAddr - entry->startVirt) >> PAGE_SIZE_SHIFT;
        Protection prot = entry->protection;
        CacheType cacheType = entry->cacheType;
        
        spinlock_acquire(&obj->lock);
        m_mapEntries.unlock();

        Page* page = obj->pages;
        for (uint64_t i = 0; i < pageIndex; i++) {
            if (page == nullptr) {
                spinlock_release(&obj->lock);
                return false;
            }
            page = page->next;
        }

        if (page->physAddr != 0) { // not mapped here, but is somewhere else
            bool result = m_pageMapper->MapPage(virtAddr, page->physAddr, prot, cacheType);
            spinlock_release(&obj->lock);
            return result;
        }

        // Check that the protection is fine
        switch (page->protection) {
        case Protection::READ:
        case Protection::WRITE:
        case Protection::EXECUTE:
            if (prot != page->protection) {
                spinlock_release(&obj->lock);
                return false;
            }
            break;
        case Protection::READ_WRITE:
            if (prot == Protection::EXECUTE || prot == Protection::READ_EXECUTE || prot == Protection::READ_WRITE_EXECUTE) {
                spinlock_release(&obj->lock);
                return false;
            }
            break;
        case Protection::READ_EXECUTE:
            if (prot == Protection::WRITE || prot == Protection::READ_WRITE || prot == Protection::READ_WRITE_EXECUTE) {
                spinlock_release(&obj->lock);
                return false;
            }
            break;
        default:
            break;
        }

        page->physAddr = reinterpret_cast<uint64_t>(obj->pager->AllocatePage());
        bool result = m_pageMapper->MapPage(virtAddr, page->physAddr, prot, cacheType);

        spinlock_release(&obj->lock);

        return result;
    }


}