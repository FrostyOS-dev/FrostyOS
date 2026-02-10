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
#include <util.h>

VMRegionAllocator::VMRegionAllocator() : m_start(UINT64_MAX), m_end(0), m_allPagesTree(true), m_freePagesTree(true), m_freePageCount(0), m_usedPageCount(0), m_reservedPageCount(0), m_totalPageCount(0) {

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

void* VMRegionAllocator::AllocatePages(uint64_t numPages) {
    uint64_t start = 0;
    AVLTree::wAVLTreeNode* node = m_freePagesTree.FindNodeOrHigher(numPages);
    if (node == nullptr)
        return nullptr;
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
    if (allPagesNode == nullptr)
        return nullptr;

    if (allPagesNode->key < start) // need to split the node
        allPagesNode = SplitAPTNode(allPagesNode, start - allPagesNode->key);

    CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    if (nodeData.isFree == 0) // already used
        return nullptr;

    if (nodeData.size > numPages) // need to split the node
        SplitAPTNode(allPagesNode, numPages);

    nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);

    nodeData.isFree = 0;
    allPagesNode->value = std::bit_cast<uint64_t>(nodeData);

    m_freePageCount -= numPages;
    m_usedPageCount += numPages;

    return (void*)start;
}

void VMRegionAllocator::FreePages(void* ptr, uint64_t numPages) {
    // Step 1: Find the exactly matching node in the m_allPagesTree, partial matches are not allowed
    AVLTree::wAVLTreeNode* allPagesNode = m_allPagesTree.FindNode((uint64_t)ptr);
    if (allPagesNode == nullptr)
        return;
    CompleteTreeNodeData nodeData = std::bit_cast<CompleteTreeNodeData>(allPagesNode->value);
    if (nodeData.isFree == 1)
        return; // already free
    if (nodeData.size != numPages)
        return; // not the right size
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
}

void VMRegionAllocator::ReservePages(void* ptr, uint64_t numPages) {
    // TODO
}

void VMRegionAllocator::UnreservePages(void* ptr, uint64_t numPages) {
    // TODO
}

uint64_t VMRegionAllocator::GetStart() const {
    return m_start;
}

uint64_t VMRegionAllocator::GetEnd() const {
    return m_end;
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