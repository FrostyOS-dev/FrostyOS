/*
Copyright (©) 2024  Frosty515

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

#include "VirtualMemoryAllocator.hpp"

#include <assert.h>
#include <spinlock.h>
#include <util.h>
#include <stdlib.h>

#include <Data-structures/LinkedList.hpp>

VirtualMemoryAllocator::VirtualMemoryAllocator() : m_freeSizeTree(true), m_allAddressTree(true), m_freePages(0), m_usedPages(0), m_reservedPages(0), m_lock(SPINLOCK_DEFAULT_VALUE) {
    
}

VirtualMemoryAllocator::VirtualMemoryAllocator(void* start, uint64_t pageCount) : VirtualMemoryAllocator() {
    AddRegion(start, pageCount);
}

VirtualMemoryAllocator::~VirtualMemoryAllocator() {
    m_freeSizeTree.clear();
    m_allAddressTree.clear();
}

void* VirtualMemoryAllocator::AllocatePages(uint64_t pageCount) {
    Verify();
    const uint64_t initialPageCount = pageCount;
    m_freeSizeTree.lock();

    uint64_t actualPageCount;
    AVLTree::Node* node = AVLTree::findNodeOrHigher(m_freeSizeTree.GetRoot(), pageCount, &actualPageCount);
    if (node == nullptr) {
        m_freeSizeTree.unlock();
        return nullptr;
    }
    LinkedList::Node* root = (LinkedList::Node*)node->extraData;
    if (root == nullptr) {
        m_freeSizeTree.unlock();
        return nullptr;
    }

    void* address = (void*)root->data;
    {
        LinkedList::Node* temp = root;
        root = root->next;
        if (root != nullptr)
            root->previous = nullptr;
        
        kfree_vmm(temp);
    }
    if (root == nullptr)
        m_freeSizeTree.remove(actualPageCount);
    else
        node->extraData = (uint64_t)root;

    m_allAddressTree.lock();
    node = AVLTree::findNode(m_allAddressTree.GetRoot(), (uint64_t)address);
    PageRegionData* data = (PageRegionData*)&(node->extraData);
    assert(data->pageCount == actualPageCount && data->free == 1 && data->reserved == 0);

    data->free = 0;
    data->reserved = 0;

    if (actualPageCount > pageCount) {
        data->pageCount = pageCount;

        PageRegionData newData = {actualPageCount - pageCount, 1, 0};
        uint64_t* temp = (uint64_t*)&newData;
        m_allAddressTree.insert((void*)((uint64_t)address + (pageCount * PAGE_SIZE)), *temp);

        node = AVLTree::findNode(m_freeSizeTree.GetRoot(), actualPageCount - pageCount);
        if (node == nullptr)
            node = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), actualPageCount - pageCount, 0, true);
        root = (LinkedList::Node*)node->extraData;
        LinkedList::insertNode(root, (uint64_t)address + (pageCount * PAGE_SIZE), false, true);
        node->extraData = (uint64_t)root;
    }

    m_allAddressTree.unlock();
    m_freeSizeTree.unlock();

    spinlock_acquire(&m_lock);
    m_usedPages += initialPageCount;
    m_freePages -= initialPageCount;
    spinlock_release(&m_lock);
    Verify();
    // dbgprintf("Allocated %lu pages at %lp\n", pageCount, address);
    return address;
}

void* VirtualMemoryAllocator::AllocatePage() {
    return AllocatePages(1);
}

void* VirtualMemoryAllocator::AllocatePages(void* address, uint64_t pageCount) {
    const uint64_t initialPageCount = pageCount;
    m_allAddressTree.lock();
    uint64_t realAddress = 0;
    AVLTree::Node* node = AVLTree::findNodeOrLower(m_allAddressTree.GetRoot(), (uint64_t)address, &realAddress);
    if (node == nullptr) {
        m_allAddressTree.unlock();
        return nullptr;
    }
    PageRegionData* data = (PageRegionData*)&(node->extraData);
    if (data->free == 0 || (data->pageCount - ((uint64_t)address - realAddress) / PAGE_SIZE) < pageCount) {
        m_allAddressTree.unlock();
        return nullptr;
    }

    // Remove it from the free size tree
    m_freeSizeTree.lock();
    AVLTree::Node* sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), data->pageCount);
    LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
    LinkedList::deleteNode(root, (uint64_t)realAddress, true);
    if (root == nullptr)
        m_freeSizeTree.remove(data->pageCount);
    else
        sizeNode->extraData = (uint64_t)root;
    m_freeSizeTree.unlock();

    if (realAddress < (uint64_t)address) {
        PageRegionData oldData = *data;
        data->pageCount = ((uint64_t)address - realAddress) / PAGE_SIZE;

        // insert to the free size tree
        m_freeSizeTree.lock();
        sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), data->pageCount);
        if (sizeNode == nullptr)
            sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), data->pageCount, 0, true);
        root = (LinkedList::Node*)sizeNode->extraData;
        LinkedList::insertNode(root, realAddress, false, true);
        sizeNode->extraData = (uint64_t)root;
        m_freeSizeTree.unlock();

        oldData.pageCount -= data->pageCount;
        oldData.free = 1;
        oldData.reserved = 0;
        uint64_t* temp = (uint64_t*)&oldData;
        node = AVLTree::insertNode(m_allAddressTree.GetRootRef(), (uint64_t)address, *temp, true);
        data = (PageRegionData*)&(node->extraData);
    }

    if (data->pageCount > pageCount) {
        PageRegionData newData = *data;
        newData.pageCount = data->pageCount - pageCount;
        uint64_t* temp = (uint64_t*)&newData;
        node = AVLTree::insertNode(m_allAddressTree.GetRootRef(), (uint64_t)address + pageCount * PAGE_SIZE, *temp, true);
        data->pageCount = pageCount;

        m_freeSizeTree.lock();
        sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), newData.pageCount);
        if (sizeNode == nullptr)
            sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), newData.pageCount, 0, true);
        root = (LinkedList::Node*)sizeNode->extraData;
        LinkedList::insertNode(root, (uint64_t)address + pageCount * PAGE_SIZE, false, true);
        sizeNode->extraData = (uint64_t)root;
        m_freeSizeTree.unlock();
    }

    data->free = 0;
    data->reserved = 0;

    m_allAddressTree.unlock();

    spinlock_acquire(&m_lock);
    m_usedPages += initialPageCount;
    m_freePages -= initialPageCount;
    spinlock_release(&m_lock);
    return address;
}

void* VirtualMemoryAllocator::AllocatePage(void* address) {
    return AllocatePages(address, 1);
}

void VirtualMemoryAllocator::FreePages(void* address, uint64_t pageCount) {
    // dbgprintf("Freeing %lu pages at %lp\n", pageCount, address);
    Verify();
    const uint64_t initialPageCount = pageCount;
    m_allAddressTree.lock();
    AVLTree::Node* node = AVLTree::findNode(m_allAddressTree.GetRoot(), (uint64_t)address);
    if (node == nullptr) {
        m_allAddressTree.unlock();
        return;
    }

    PageRegionData* data = (PageRegionData*)&(node->extraData);
    if (data->free == 1 || data->pageCount != pageCount) {
        m_allAddressTree.unlock();
        assert(false); // temporary
        return;
    }
    data->free = 1;

    // struct i_PageRegion {
    //     uint64_t pageCount;
    //     uint64_t address;
    // } regionsForRemoval[2];
    // uint64_t regionsForRemovalCount = 0;

    m_freeSizeTree.lock();
    // // first find the previous region
    // uint64_t realAddress = 0;
    // AVLTree::Node* prevNode = AVLTree::findNodeOrLower(m_allAddressTree.GetRoot(), (uint64_t)address - 1, &realAddress);
    // if (prevNode != nullptr) {
    //     PageRegionData* prevData = (PageRegionData*)&(prevNode->extraData);
    //     if (prevData->free == 1 && prevData->reserved == 0 && ((uint64_t)realAddress + prevData->pageCount * PAGE_SIZE) == (uint64_t)address) {
    //         regionsForRemoval[regionsForRemovalCount].pageCount = prevData->pageCount;
    //         regionsForRemoval[regionsForRemovalCount].address = realAddress;
    //         regionsForRemovalCount++;

    //         // remove the current node from the all address tree
    //         m_allAddressTree.remove(address);

    //         // add the page count to the previous node
    //         prevData->pageCount += pageCount;

    //         address = (void*)realAddress;
    //         pageCount = prevData->pageCount;
    //         data = prevData;
    //     }
    // }

    // // now find the next region
    // realAddress = 0;
    // AVLTree::Node* nextNode = AVLTree::findNode(m_allAddressTree.GetRoot(), (uint64_t)address + pageCount * PAGE_SIZE);
    // if (nextNode != nullptr) {
    //     PageRegionData* nextData = (PageRegionData*)&(nextNode->extraData);
    //     if (nextData->free == 1 && nextData->reserved == 0) {
    //         regionsForRemoval[regionsForRemovalCount].pageCount = nextData->pageCount;
    //         regionsForRemoval[regionsForRemovalCount].address = (uint64_t)address + pageCount * PAGE_SIZE;
    //         regionsForRemovalCount++;

    //         uint64_t nextPageCount = nextData->pageCount;

    //         // remove the next node from the all address tree
    //         m_allAddressTree.remove((void*)((uint64_t)address + pageCount * PAGE_SIZE));

    //         // add the page count to the previous node
    //         data->pageCount += nextPageCount;

    //         pageCount = data->pageCount;
    //     }
    // }

    m_allAddressTree.unlock();

    AVLTree::Node* sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), pageCount);
    if (sizeNode == nullptr)
        sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), pageCount, 0, true);
    LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
    LinkedList::insertNode(root, (uint64_t)address, false, true);
    sizeNode->extraData = (uint64_t)root;

    // if (regionsForRemovalCount == 2 && regionsForRemoval[0].pageCount == regionsForRemoval[1].pageCount) { // simple optimisation
    //     sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), regionsForRemoval[0].pageCount);
    //     if (sizeNode != nullptr) {
    //         LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
    //         LinkedList::deleteNode(root, regionsForRemoval[0].address, true);
    //         LinkedList::deleteNode(root, regionsForRemoval[1].address, true);
    //         if (root == nullptr)
    //             m_freeSizeTree.remove(regionsForRemoval[0].pageCount);
    //         else
    //             sizeNode->extraData = (uint64_t)root;
    //     }
    // }
    // else {
        // for (uint64_t i = 0; i < regionsForRemovalCount; i++) {
        //     sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), regionsForRemoval[i].pageCount);
        //     if (sizeNode != nullptr) {
        //         LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
        //         LinkedList::deleteNode(root, regionsForRemoval[i].address, true);
        //         if (root == nullptr)
        //             m_freeSizeTree.remove(regionsForRemoval[i].pageCount);
        //         else
        //             sizeNode->extraData = (uint64_t)root;
        //     }
        // }
    // }
    m_freeSizeTree.unlock();

    spinlock_acquire(&m_lock);
    m_usedPages -= initialPageCount;
    m_freePages += initialPageCount;
    spinlock_release(&m_lock);
    Verify();
}

void VirtualMemoryAllocator::FreePage(void* address) {
    FreePages(address, 1);
}

bool VirtualMemoryAllocator::ReservePages(void* address, uint64_t pageCount) {
    const uint64_t initialPageCount = pageCount;
    m_allAddressTree.lock();
    uint64_t realAddress = 0;
    AVLTree::Node* node = AVLTree::findNodeOrLower(m_allAddressTree.GetRoot(), (uint64_t)address, &realAddress);
    if (node == nullptr) {
        m_allAddressTree.unlock();
        return false;
    }
    PageRegionData* data = (PageRegionData*)&(node->extraData);
    if (data->free == 0 || (data->pageCount - ((uint64_t)address - realAddress) / PAGE_SIZE) < pageCount) {
        m_allAddressTree.unlock();
        return false;
    }
    else if (data->reserved == 1) { // already reserved
        m_allAddressTree.unlock();
        return true;
    }

    // Remove it from the free size tree
    m_freeSizeTree.lock();
    AVLTree::Node* sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), data->pageCount);
    LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
    LinkedList::deleteNode(root, (uint64_t)realAddress, true);
    if (root == nullptr)
        m_freeSizeTree.remove(data->pageCount);
    else
        sizeNode->extraData = (uint64_t)root;
    m_freeSizeTree.unlock();

    if (realAddress < (uint64_t)address) {
        PageRegionData oldData = *data;
        data->pageCount = ((uint64_t)address - realAddress) / PAGE_SIZE;

        // insert to the free size tree
        m_freeSizeTree.lock();
        sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), data->pageCount);
        if (sizeNode == nullptr)
            sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), data->pageCount, 0, true);
        root = (LinkedList::Node*)sizeNode->extraData;
        LinkedList::insertNode(root, realAddress, false, true);
        sizeNode->extraData = (uint64_t)root;
        m_freeSizeTree.unlock();

        oldData.pageCount -= data->pageCount;
        oldData.free = 1;
        oldData.reserved = 0;
        uint64_t* temp = (uint64_t*)&oldData;
        node = AVLTree::insertNode(m_allAddressTree.GetRootRef(), (uint64_t)address, *temp, true);
        data = (PageRegionData*)&(node->extraData);
    }

    if (data->pageCount > pageCount) {
        PageRegionData newData = *data;
        newData.pageCount = data->pageCount - pageCount;
        uint64_t* temp = (uint64_t*)&newData;
        node = AVLTree::insertNode(m_allAddressTree.GetRootRef(), (uint64_t)address + pageCount * PAGE_SIZE, *temp, true);
        data->pageCount = pageCount;

        m_freeSizeTree.lock();
        sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), newData.pageCount);
        if (sizeNode == nullptr)
            sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), newData.pageCount, 0, true);
        root = (LinkedList::Node*)sizeNode->extraData;
        LinkedList::insertNode(root, (uint64_t)address + pageCount * PAGE_SIZE, false, true);
        sizeNode->extraData = (uint64_t)root;
        m_freeSizeTree.unlock();
    }

    data->free = 0;
    data->reserved = 1;

    m_allAddressTree.unlock();

    spinlock_acquire(&m_lock);
    m_reservedPages += initialPageCount;
    m_freePages -= initialPageCount;
    spinlock_release(&m_lock);
    return true;
}

void VirtualMemoryAllocator::UnreservePages(void* address, uint64_t pageCount) {
    const uint64_t initialPageCount = pageCount;
    m_allAddressTree.lock();
    AVLTree::Node* node = AVLTree::findNode(m_allAddressTree.GetRoot(), (uint64_t)address);
    if (node == nullptr) {
        m_allAddressTree.unlock();
        return;
    }

    PageRegionData* data = (PageRegionData*)&(node->extraData);
    if (data->reserved == 0 || data->pageCount != pageCount) {
        m_allAddressTree.unlock();
        return;
    }
    data->reserved = 0;
    data->free = 1;

    struct i_PageRegion {
        uint64_t pageCount;
        uint64_t address;
    } regionsForRemoval[2];
    uint64_t regionsForRemovalCount = 0;

    m_freeSizeTree.lock();
    // first find the previous region
    uint64_t realAddress = 0;
    AVLTree::Node* prevNode = AVLTree::findNodeOrLower(m_allAddressTree.GetRoot(), (uint64_t)address - 1, &realAddress);
    if (prevNode != nullptr) {
        PageRegionData* prevData = (PageRegionData*)&(prevNode->extraData);
        if (prevData->free == 1 && prevData->reserved == 0 && ((uint64_t)realAddress + prevData->pageCount * PAGE_SIZE) == (uint64_t)address) {
            regionsForRemoval[regionsForRemovalCount].pageCount = prevData->pageCount;
            regionsForRemoval[regionsForRemovalCount].address = realAddress;
            regionsForRemovalCount++;

            // remove the current node from the all address tree
            m_allAddressTree.remove(address);

            // add the page count to the previous node
            prevData->pageCount += pageCount;

            address = (void*)realAddress;
            pageCount = prevData->pageCount;
            data = prevData;
        }
    }

    // now find the next region
    realAddress = 0;
    AVLTree::Node* nextNode = AVLTree::findNode(m_allAddressTree.GetRoot(), (uint64_t)address + pageCount * PAGE_SIZE);
    if (nextNode != nullptr) {
        PageRegionData* nextData = (PageRegionData*)&(nextNode->extraData);
        if (nextData->free == 1 && nextData->reserved == 0) {
            regionsForRemoval[regionsForRemovalCount].pageCount = nextData->pageCount;
            regionsForRemoval[regionsForRemovalCount].address = (uint64_t)address + pageCount * PAGE_SIZE;
            regionsForRemovalCount++;

            uint64_t nextPageCount = nextData->pageCount;

            // remove the next node from the all address tree
            m_allAddressTree.remove((void*)((uint64_t)address + pageCount * PAGE_SIZE));

            // add the page count to the previous node
            data->pageCount += nextPageCount;

            pageCount = data->pageCount;
        }
    }

    m_allAddressTree.unlock();

    AVLTree::Node* sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), pageCount);
    if (sizeNode == nullptr)
        sizeNode = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), pageCount, 0, true);
    LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
    LinkedList::insertNode(root, (uint64_t)address, false, true);
    sizeNode->extraData = (uint64_t)root;

    if (regionsForRemovalCount == 2 && regionsForRemoval[0].pageCount == regionsForRemoval[1].pageCount) {
        sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), regionsForRemoval[0].pageCount);
        if (sizeNode != nullptr) {
            LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
            LinkedList::deleteNode(root, regionsForRemoval[0].address, true);
            LinkedList::deleteNode(root, regionsForRemoval[1].address, true);
            if (root == nullptr)
                m_freeSizeTree.remove(regionsForRemoval[0].pageCount);
            else
                sizeNode->extraData = (uint64_t)root;
        }
    }
    else {
        for (uint64_t i = 0; i < regionsForRemovalCount; i++) {
            sizeNode = AVLTree::findNode(m_freeSizeTree.GetRoot(), regionsForRemoval[i].pageCount);
            if (sizeNode != nullptr) {
                LinkedList::Node* root = (LinkedList::Node*)sizeNode->extraData;
                LinkedList::deleteNode(root, regionsForRemoval[i].address, true);
                if (root == nullptr)
                    m_freeSizeTree.remove(regionsForRemoval[i].pageCount);
                else
                    sizeNode->extraData = (uint64_t)root;
            }
        }
    }
    m_freeSizeTree.unlock();

    spinlock_acquire(&m_lock);
    m_reservedPages -= initialPageCount;
    m_freePages += initialPageCount;
    spinlock_release(&m_lock);
}

void VirtualMemoryAllocator::AddRegion(void* address, uint64_t pageCount) {
    union {
        uint64_t raw;
        PageRegionData data;
    } u = {.data = {pageCount, 1, 0}}; // looks weird, but prevents a warning
    m_allAddressTree.lock();
    m_allAddressTree.insert(address, u.raw);
    m_allAddressTree.unlock();
    m_freeSizeTree.lock();
    AVLTree::Node* node = AVLTree::findNode(m_freeSizeTree.GetRoot(), pageCount);
    if (node == nullptr)
        node = AVLTree::insertNode(m_freeSizeTree.GetRootRef(), pageCount, 0, true);
    LinkedList::Node* root = (LinkedList::Node*)node->extraData;
    LinkedList::insertNode(root, (uint64_t)address, false, true);
    node->extraData = (uint64_t)root;
    m_freeSizeTree.unlock();
    spinlock_acquire(&m_lock);
    m_freePages += pageCount;
    spinlock_release(&m_lock);
}

void VirtualMemoryAllocator::Verify() {
    // This function is for testing purposes only. Should not be used in normal circumstances.
    // struct i_PageRegion {
    //     uint64_t pageCount;
    //     uint64_t address;
    // };
    // LinkedList::SimpleLinkedList<i_PageRegion> regions(true, true);
    struct Data {
        // LinkedList::SimpleLinkedList<i_PageRegion>* regions;
        VirtualMemoryAllocator* vma;
    } data = {/*&regions,*/ this};
    m_freeSizeTree.Enumerate([](uint64_t key, LinkedList::Node* root, void* data) -> void {
        Data* d = (Data*)data;
        VirtualMemoryAllocator* vma = d->vma;
        // LinkedList::SimpleLinkedList<i_PageRegion>* regions = d->regions;
        while (root != nullptr) {
            void* address = (void*)root->data;
            // for (uint64_t i = 0; i < regions->getCount(); i++) {
            //     i_PageRegion* region = regions->get(i);
            //     if (region->address == (uint64_t)address) {
            //         dbgprintf("Free size tree: address %lp already exists in regions list\n", address);
            //         assert(false);
            //         return;
            //     }
            //     if (!((region->address < (uint64_t)address && (region->address + region->pageCount * PAGE_SIZE) <= (uint64_t)address) || ((uint64_t)address < region->address && ((uint64_t)address + key * PAGE_SIZE) <= region->address))) {
            //         dbgprintf("Free size tree: address %lp, size %lu overlaps with region: address %lp, size %lp\n", address, key, region->address, region->pageCount);
            //         assert(false);
            //         return;
            //     }
            // }
            // {
            //     i_PageRegion* region = (i_PageRegion*)kcalloc_vmm(1, sizeof(i_PageRegion));
            //     region->pageCount = key;
            //     region->address = (uint64_t)address;
            //     regions->insert(region);
            // }
            AVLTree::Node* node = AVLTree::findNode(vma->m_allAddressTree.GetRoot(), (uint64_t)address);
            if (node == nullptr) {
                dbgprintf("Free size tree: address %lp not found in all address tree\n", address);
                assert(false);
                return;
            }
            PageRegionData* data = (PageRegionData*)&(node->extraData);
            if (data->free == 0) {
                dbgprintf("Free size tree: address %lp is not free\nEnumerating all address tree:\n", address);
                vma->m_allAddressTree.Enumerate([](void* address, uint64_t extraData, void*) {
                    PageRegionData* data = (PageRegionData*)&extraData;
                    dbgprintf("address: %lp, pageCount: %lu, free: %d, reserved: %d\n", address, data->pageCount, data->free, data->reserved);
                }, nullptr);
                dbgprintf("\n");
                assert(false);
                return;
            }
            if (data->pageCount != key) {
                dbgprintf("Free size tree: address %lp has pageCount %lu, expected %lu\n", address, data->pageCount, key);
                assert(false);
                return;
            }
            root = root->next;
        }
    }, &data);
    // {
    //     uint64_t count = regions.getCount();
    //     for (uint64_t i = 0; i < count; i++) {
    //         i_PageRegion* region = regions.get(0);
    //         kfree_vmm(region);
    //         regions.remove(0UL);
    //     }
    // }
    // m_allAddressTree.Enumerate([](void* address, uint64_t extraData, void* data) -> void {
    //     PageRegionData* pageRegion = (PageRegionData*)&extraData;
    //     Data* d = (Data*)data;
    //     // VirtualMemoryAllocator* vma = d->vma;
    //     LinkedList::SimpleLinkedList<i_PageRegion>* regions = d->regions;
    //     for (uint64_t i = 0; i < regions->getCount(); i++) {
    //         i_PageRegion* region = regions->get(i);
    //         if (!((region->address < (uint64_t)address && (region->address + region->pageCount * PAGE_SIZE) <= (uint64_t)address) || ((uint64_t)address < region->address && ((uint64_t)address + pageRegion->pageCount * PAGE_SIZE) <= region->address))) {
    //             dbgprintf("All address tree: address %lp, size %lu overlaps with region: address %lp, size %lp\n", address, pageRegion->pageCount, region->address, region->pageCount);
    //             assert(false);
    //             return;
    //         }
    //     }
    //     {
    //         i_PageRegion* region = (i_PageRegion*)kcalloc_vmm(1, sizeof(i_PageRegion));
    //         region->pageCount = pageRegion->pageCount;
    //         region->address = (uint64_t)address;
    //         regions->insert(region);
    //     }
    // }, &data);
    // {
    //     uint64_t count = regions.getCount();
    //     for (uint64_t i = 0; i < count; i++) {
    //         i_PageRegion* region = regions.get(0);
    //         kfree_vmm(region);
    //         regions.remove(0UL);
    //     }
    // }
}

VirtualMemoryAllocator* g_GlobalVMA = nullptr;
VirtualMemoryAllocator* g_KVMA = nullptr;
VirtualMemoryAllocator* g_KExecVMA = nullptr;
