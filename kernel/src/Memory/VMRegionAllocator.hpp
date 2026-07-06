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

#ifndef _VIRTMEM_REGION_ALLOCATOR_HPP
#define _VIRTMEM_REGION_ALLOCATOR_HPP

#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

class VMRegionAllocator {
public:
    VMRegionAllocator();
    ~VMRegionAllocator();

    void Init(uint64_t start, uint64_t end);
    void Delete();

    void* AllocatePages(uint64_t numPages);
    void* AllocatePages(void* ptr, uint64_t numPages);
    void FreePages(void* ptr, uint64_t numPages, bool exact = true);

    void ReservePages(void* ptr, uint64_t numPages);
    void UnreservePages(void* ptr, uint64_t numPages);

    bool ResizeAllocatedRegion(void* ptr, uint64_t numPages, void* newStart, uint64_t newNumPages);

    uint64_t GetStart() const;
    uint64_t GetEnd() const;

private:
    // Split a node in the allPagesTree, returning the new node for the upper half. numPage is the number of pages to split at into the old node.
    AVLTree::wAVLTreeNode* SplitAPTNode(AVLTree::wAVLTreeNode* node, uint64_t numPages);

    void Verify();

private:
    struct [[gnu::packed, gnu::aligned(8)]] CompleteTreeNodeData {
        uint64_t size : 62;
        uint64_t isFree : 1;
        uint64_t isReserved : 1;
    };

    uint64_t m_start;
    uint64_t m_end;
    AVLTree::wAVLTree<uint64_t, uint64_t> m_allPagesTree;
    AVLTree::wAVLTree<uint64_t, LinkedList::Node*> m_freePagesTree;

    uint64_t m_freePageCount;
    uint64_t m_usedPageCount;
    uint64_t m_reservedPageCount;
    uint64_t m_totalPageCount;

    // spinlock_t m_lock;
    Mutex m_lock;
};

// Implemented in arch specific code
void GetDefaultUserRegion(uint64_t* start, uint64_t* end);
bool IsInUserRegion(uint64_t addr);

#endif /* _VIRTMEM_REGION_ALLOCATOR_HPP */