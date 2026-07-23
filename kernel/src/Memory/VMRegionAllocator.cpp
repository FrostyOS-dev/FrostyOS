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

#include "VMRegionAllocator.hpp"

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <bit>
#include <cstdint>
#include <spinlock.h>
#include <util.h>

VMRegionAllocator::VMRegionAllocator() : m_start(UINT64_MAX), m_end(0), m_allPagesTree(true), m_freePagesTree(true), m_freePageCount(0), m_usedPageCount(0), m_reservedPageCount(0), m_totalPageCount(0), m_lock() {

}

VMRegionAllocator::~VMRegionAllocator() {

}

void VMRegionAllocator::Init(uint64_t start, uint64_t end) {
    m_start = ALIGN_DOWN(start, PAGE_SIZE);
    m_end = ALIGN_UP(end, PAGE_SIZE);
    uint64_t pageCount;

    if (m_start == 0 && m_end == 0) // all of memory
        pageCount = 1UL << (64 - PAGE_SIZE_SHIFT);
    else
        pageCount = (m_end - m_start) >> PAGE_SIZE_SHIFT;

    CompleteTreeNodeData nodeData = {pageCount, 1, 0};

    m_allPagesTree.Insert(m_start, std::bit_cast<uint64_t>(nodeData));

    LinkedList::Node* node = nullptr;
    LinkedList::insertNode(node, m_start, true);

    m_freePagesTree.Insert(pageCount, node);

    m_freePageCount = pageCount;
    m_usedPageCount = 0;
    m_reservedPageCount = 0;
    m_totalPageCount = pageCount;
}

void VMRegionAllocator::Delete() {
    m_lock.Lock();

    // Step 1: clear the linked list at each node
    m_freePagesTree.forEach([](void*, uint64_t, LinkedList::Node* list) -> bool {
        while (list != nullptr) // clear the list
            LinkedList::deleteNode(list, list, true);
        return true;
    }, nullptr);

    // Step 2: Clear both wAVL trees
    m_freePagesTree.Clear();
    m_allPagesTree.Clear();

    m_freePageCount = 0;
    m_usedPageCount = 0;
    m_reservedPageCount = 0;
    m_totalPageCount = 0;

    m_lock.Unlock();
}

void* VMRegionAllocator::AllocatePages(uint64_t numPages) {
    m_lock.Lock();
    uint64_t start = 0;
    AVLTree::wAVLTreeNode* node = m_freePagesTree.FindNodeOrHigher(numPages);
    if (node == nullptr) {
        m_lock.Unlock();
        return nullptr;
    }
    if (node->key > numPages) {
        // need to split the node
        uint64_t newNodePageCount = node->key - numPages;

        // remove the node from the free pages tree
        LinkedList::Node* freeNode = (LinkedList::Node*)node->value;
        start = freeNode->data;
        
        LinkedList::deleteNode(freeNode, freeNode, true);
        if (freeNode == nullptr)
            m_freePagesTree.RemoveNode(node);
        else
            node->value = (uint64_t)freeNode;

        // insert a new region for the remaining pages
        AVLTree::wAVLTreeNode* newNode = m_freePagesTree.FindNode(newNodePageCount);
        if (newNode == nullptr)
            newNode = m_freePagesTree.Insert(newNodePageCount, nullptr);

        LinkedList::insertNode((LinkedList::Node*&)newNode->value, start + (numPages << PAGE_SIZE_SHIFT), true);
    }
    else {
        LinkedList::Node* freeNode = (LinkedList::Node*)node->value;
        start = freeNode->data;
        
        LinkedList::deleteNode(freeNode, freeNode, true);
        if (freeNode == nullptr)
            m_freePagesTree.RemoveNode(node);
        else
            node->value = (uint64_t)freeNode;
    }

    // Next step is to find the node in the m_allPagesTree and split from bigger section if needed, then mark as used
    AVLTree::wAVLTreeNode* allPagesNode = m_allPagesTree.FindNodeOrLower(start);
    if (allPagesNode == nullptr) {
        m_lock.Unlock();
        return nullptr;
    }

    if (allPagesNode->key < start) // need to split the node
        allPagesNode = SplitAPTNode(allPagesNode, (start - allPagesNode->key) >> PAGE_SIZE_SHIFT);

    CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    if (nodeData.isFree == 0) { // already used
        m_lock.Unlock();
        return nullptr;
    }

    if (nodeData.size > numPages) // need to split the node
        SplitAPTNode(allPagesNode, numPages);

    nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);

    nodeData.isFree = 0;
    allPagesNode->value = std::bit_cast<uint64_t>(nodeData);

    m_freePageCount -= numPages;
    m_usedPageCount += numPages;

    m_lock.Unlock();

    return (void*)start;
}

void* VMRegionAllocator::AllocatePages(void* ptr, uint64_t numPages, bool lock) {
    if (lock)
        m_lock.Lock();

    // Step 1: Find the all pages tree node
    AVLTree::wAVLTreeNode* allPagesNode = m_allPagesTree.FindNodeOrLower((uint64_t)ptr);
    if (allPagesNode == nullptr) {
        if (lock)
            m_lock.Unlock();
        return nullptr;
    }
    CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    if (nodeData.isFree == 0 || allPagesNode->key + (nodeData.size << PAGE_SIZE_SHIFT) < (uint64_t)ptr + (numPages << PAGE_SIZE_SHIFT)) {
        if (lock)
            m_lock.Unlock();
        return nullptr;
    }

    // Step 2: Remove the free pages tree node
    AVLTree::wAVLTreeNode* freePagesNode = m_freePagesTree.FindNode(nodeData.size);
    if (freePagesNode == nullptr) {
        if (lock)
            m_lock.Unlock();
        return nullptr;
    }

    LinkedList::Node* list = (LinkedList::Node*)freePagesNode->value;
    LinkedList::deleteNode(list, allPagesNode->key);
    if (list == nullptr)
        m_freePagesTree.RemoveNode(freePagesNode);
    else
        freePagesNode->value = (uint64_t)list;

    // Step 3: Isolate the required section of the region
    if (allPagesNode->key < (uint64_t)ptr) {
        uint64_t pageCount = ((uint64_t)ptr - allPagesNode->key) >> PAGE_SIZE_SHIFT;
        AVLTree::wAVLTreeNode* node = SplitAPTNode(allPagesNode, pageCount);
        assert(node != nullptr);

        freePagesNode = m_freePagesTree.FindNode(pageCount);
        if (freePagesNode == nullptr) {
            freePagesNode = m_freePagesTree.Insert(pageCount, nullptr);
            assert(freePagesNode != nullptr);
        }

        LinkedList::insertNode((LinkedList::Node*&)freePagesNode->value, allPagesNode->key);

        allPagesNode = node;
        nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    }

    if (nodeData.size > numPages) {
        AVLTree::wAVLTreeNode* node = SplitAPTNode(allPagesNode, numPages);
        assert(node != nullptr);

        CompleteTreeNodeData newNodeData = std::bit_cast<CompleteTreeNodeData>(node->value);

        freePagesNode = m_freePagesTree.FindNode(newNodeData.size);
        if (freePagesNode == nullptr) {
            freePagesNode = m_freePagesTree.Insert(newNodeData.size, nullptr);
            assert(freePagesNode != nullptr);
        }

        LinkedList::insertNode((LinkedList::Node*&)freePagesNode->value, node->key);

        nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    }

    nodeData.isFree = 0;
    allPagesNode->value = std::bit_cast<uint64_t>(nodeData);

    m_freePageCount -= numPages;
    m_usedPageCount += numPages;

    if (lock)
        m_lock.Unlock();

    return ptr;
}

void VMRegionAllocator::FreePages(void* ptr, uint64_t numPages, bool exact, bool lock) {
    if (lock)
        m_lock.Lock();
    // Step 1: Find the exactly matching node in the m_allPagesTree if exact is specificied, if not, just a containing region
    AVLTree::wAVLTreeNode* allPagesNode = nullptr;
    CompleteTreeNodeData nodeData;
    if (exact) {
        allPagesNode = m_allPagesTree.FindNode((uint64_t)ptr);
        if (allPagesNode == nullptr) {
            if (lock)
                m_lock.Unlock();
            return;
        }
        nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
        if (nodeData.isFree == 1 || nodeData.size != numPages) {
            if (lock)
                m_lock.Unlock();
            return; // already free or not the right size
        }
    } else {
        allPagesNode = m_allPagesTree.FindNodeOrLower((uint64_t)ptr);
        if (allPagesNode == nullptr) {
            if (lock)
                m_lock.Unlock();
            return;
        }
        nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
        if (nodeData.isFree == 1 || allPagesNode->key + nodeData.size * PAGE_SIZE < (uint64_t)ptr + numPages * PAGE_SIZE) { // already free or wrong size
            if (lock)
                m_lock.Unlock();
            return;
        }
        if (allPagesNode->key < (uint64_t)ptr) {
            allPagesNode = SplitAPTNode(allPagesNode, ((uint64_t)ptr - allPagesNode->key) >> PAGE_SIZE_SHIFT);
            nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
        }
        
        if (nodeData.size != numPages) {
            SplitAPTNode(allPagesNode, numPages);
            nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
        }
    }

    nodeData.isFree = 1;
    allPagesNode->value = std::bit_cast<uint64_t>(nodeData);

    // Step 2: merge with neighbouring free nodes
    struct Region {
        void* start;
        uint64_t size;
    };

    Region regionToInsert = {(void*)ptr, numPages};
    Region regionsToRemove[2] = {{nullptr, 0}, {nullptr, 0}};
    uint8_t regionCount = 0;

    AVLTree::wAVLTreeNode* previous = m_allPagesTree.PreviousNode(allPagesNode);
    if (previous != nullptr) {
        CompleteTreeNodeData previousNodeData = std::bit_cast<CompleteTreeNodeData>(previous->value);
        if (previousNodeData.isFree == 1 && previous->key + (previousNodeData.size << PAGE_SIZE_SHIFT) == (uint64_t)ptr) {
            // merge with previous node
            regionsToRemove[0].start = (void*)previous->key;
            regionsToRemove[0].size = previousNodeData.size;
            regionToInsert.start = regionsToRemove[0].start;
            regionToInsert.size += regionsToRemove[0].size;
            regionCount++;

            // remove the previous node from the tree
            m_allPagesTree.RemoveNode(previous);

            // remove the current node
            m_allPagesTree.RemoveNode(allPagesNode);
        }
    }

    // can't use the old node as the tree might have been modified
    AVLTree::wAVLTreeNode* next = m_allPagesTree.FindNodeOrHigher((uint64_t)ptr + (numPages << PAGE_SIZE_SHIFT));
    if (next != nullptr) {
        CompleteTreeNodeData nextNodeData = std::bit_cast<CompleteTreeNodeData>(next->value);
        if (nextNodeData.isFree == 1 && (uint64_t)ptr + (numPages << PAGE_SIZE_SHIFT) == next->key) {
            // merge with next node
            regionsToRemove[regionCount].start = (void*)next->key;
            regionsToRemove[regionCount].size = nextNodeData.size;
            regionToInsert.size += regionsToRemove[regionCount].size;
            regionCount++;

            // remove the next node from the tree
            m_allPagesTree.RemoveNode(next);

            if (regionCount == 1) // remove the current node from the tree if it hasn't already
                m_allPagesTree.RemoveNode(allPagesNode);
        }
    }

    // Step 3: If we merged with at least one neighbour, we need to insert the new node
    if (regionCount >= 1) {
        CompleteTreeNodeData newNodeData = { regionToInsert.size, 1, 0 };
        allPagesNode = m_allPagesTree.Insert((uint64_t)regionToInsert.start, std::bit_cast<uint64_t>(newNodeData));
    }

    // Step 4: Remove the regions from the free pages tree
    for (uint8_t i = 0; i < regionCount; i++) {
        AVLTree::wAVLTreeNode* node = m_freePagesTree.FindNode(regionsToRemove[i].size);
        if (node == nullptr)
            continue;
        LinkedList::Node* freeNode = (LinkedList::Node*)node->value;
        LinkedList::deleteNode(freeNode, (uint64_t)regionsToRemove[i].start, true);
        if (freeNode == nullptr)
            m_freePagesTree.RemoveNode(node);
        else
            node->value = (uint64_t)freeNode;
    }

    // Step 5: Insert the new region into the free pages tree
    AVLTree::wAVLTreeNode* node = m_freePagesTree.FindNode(regionToInsert.size);
    if (node == nullptr) // no matching node, insert a new one
        node = m_freePagesTree.Insert(regionToInsert.size, nullptr);
    LinkedList::Node* freeNode = (LinkedList::Node*)node->value;
    LinkedList::insertNode(freeNode, (uint64_t)regionToInsert.start, true);
    {
        // testing only
        LinkedList::Node* listNode = LinkedList::findNode(freeNode, (uint64_t)regionToInsert.start);
        assert(listNode != nullptr);
        assert(listNode->data == (uint64_t)regionToInsert.start);
    }
    node->value = (uint64_t)freeNode;

    // Step 6: Update the free page count
    m_freePageCount += numPages;
    m_usedPageCount -= numPages;

    if (lock)
        m_lock.Unlock();
}

void VMRegionAllocator::ReservePages(void* ptr, uint64_t numPages) {
    // TODO
}

void VMRegionAllocator::UnreservePages(void* ptr, uint64_t numPages) {
    // TODO
}

bool VMRegionAllocator::ResizeAllocatedRegion(void* ptr, uint64_t numPages, void* newStart, uint64_t newNumPages) {
    m_lock.Lock();
    // Step 1: Find the exactly matching node in the m_allPagesTree
    AVLTree::wAVLTreeNode* allPagesNode = nullptr;
    CompleteTreeNodeData nodeData;
    allPagesNode = m_allPagesTree.FindNode((uint64_t)ptr);
    if (allPagesNode == nullptr) {
        m_lock.Unlock();
        return false;
    }
    nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    if (nodeData.isFree == 1 || nodeData.size != numPages) {
        m_lock.Unlock();
        return false; // free or not the right size
    }

    if (ptr < newStart) {
        allPagesNode = SplitAPTNode(allPagesNode, ((uint64_t)newStart - (uint64_t)ptr) >> PAGE_SIZE_SHIFT);
        nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    }
    
    if (nodeData.size != newNumPages)
        SplitAPTNode(allPagesNode, newNumPages);

    m_lock.Unlock();
    return true;
}

bool VMRegionAllocator::Fork(VMRegionAllocator* other) {
    other->m_lock.Lock();
    m_lock.Lock();

    // start with the allPagesTree
    other->m_allPagesTree.forEach([](void* data, uint64_t k, uint64_t d) -> void {
        VMRegionAllocator* vma = static_cast<VMRegionAllocator*>(data);
        vma->m_allPagesTree.Insert(k, d);
    }, this);

    // now for the freePagesTree
    other->m_freePagesTree.forEach([](void* data, uint64_t k, LinkedList::Node* list) -> void {
        VMRegionAllocator* vma = static_cast<VMRegionAllocator*>(data);
        LinkedList::Node* newList = nullptr;
        while (list != nullptr) {
            LinkedList::insertNode(newList, list->data, true);
            list = list->next;
        }
        vma->m_freePagesTree.Insert(k, newList);
    }, this);

    // now everything else
    m_start = other->m_start;
    m_end = other->m_end;
    m_freePageCount = other->m_freePageCount;
    m_usedPageCount = other->m_usedPageCount;
    m_reservedPageCount = other->m_reservedPageCount;
    m_totalPageCount = other->m_totalPageCount;

    m_lock.Unlock();
    other->m_lock.Unlock();
    return true;
}

uint64_t VMRegionAllocator::GetStart() const {
    return m_start;
}

uint64_t VMRegionAllocator::GetEnd() const {
    return m_end;
}

void VMRegionAllocator::Lock() {
    m_lock.Lock();
}

void VMRegionAllocator::Unlock() {
    m_lock.Unlock();
}

AVLTree::wAVLTreeNode* VMRegionAllocator::SplitAPTNode(AVLTree::wAVLTreeNode* node, uint64_t numPages) {
    CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(node->value);
    uint64_t oldPageCount = nodeData.size;
    nodeData.size = numPages;
    node->value = std::bit_cast<uint64_t>(nodeData);

    CompleteTreeNodeData newNodeData = nodeData;
    newNodeData.size = oldPageCount - numPages;

    return m_allPagesTree.Insert(node->key + (numPages << PAGE_SIZE_SHIFT), std::bit_cast<uint64_t>(newNodeData));
}

void VMRegionAllocator::Verify() {
    assert(m_start != UINT64_MAX);
    assert(m_end != 0);
    assert(m_start < m_end);
    assert(m_freePageCount + m_usedPageCount + m_reservedPageCount == m_totalPageCount);

    m_allPagesTree.forEach([](void* data, uint64_t key, uint64_t value) -> void {
        auto* freePagesTree = (AVLTree::wAVLTree<uint64_t, LinkedList::Node*>*)data;
        CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(value);
        assert(nodeData.size > 0);
        
        // find the node in the free pages tree
        if (nodeData.isFree == 1) {
            AVLTree::wAVLTreeNode* node = freePagesTree->FindNode(nodeData.size);
            assert(node != nullptr);
            LinkedList::Node* freeNode = (LinkedList::Node*)node->value;
            assert(freeNode != nullptr);
            LinkedList::Node* listNode = LinkedList::findNode(freeNode, key);
            assert(listNode != nullptr);
            assert(listNode->data == key);
        }
    }, &m_freePagesTree);
}