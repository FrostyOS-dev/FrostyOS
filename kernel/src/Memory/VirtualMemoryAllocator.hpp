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

#ifndef _VIRTUAL_MEMORY_ALLOCATOR_HPP
#define _VIRTUAL_MEMORY_ALLOCATOR_HPP

#include <stdint.h>
#include <spinlock.h>

#include <Data-structures/AVLTree.hpp>
#include <Data-structures/LinkedList.hpp>

/*
How the allocator works:
- There are two AVL trees, one for free pages and one for all pages.

Free pages AVL tree:
- The key is the size of the regions.
- The value is a doubly linked list of starting addresses of the regions.

All pages AVL tree:
- The key is the starting address of the region.
- The value is the size of the region and 2 flags, one indicating if the region is reserved, and the other indicating if the region is free.

Allocation:
- When a region is allocated, the allocator searches for a region that is big enough in the free pages AVL tree.
- If the region is bigger than the requested size, the allocator splits the region into two parts.
- The first part is used to satisfy the request and the second part is added to the free pages AVL tree.
- The region is split accordingly in the all pages AVL tree, with the first part being updated to be used and the second part being added as a new node.

Allocation with a specific address:
- When a region is allocated with a specific address, the allocator searches for the region in the all pages AVL tree.
- If the region is found, the allocator marks the region as used in the all pages AVL tree and splits in accordingly.
- The region is removed from the free pages AVL tree.

Deallocation:
- When a region is deallocated, the allocator searches for the region in the all pages AVL tree.
- If the region is found, the allocator marks the region as free in the all pages AVL tree, and merges it with surrounding free blocks.
- The free pages AVL tree is updated accordingly.
*/

class VirtualMemoryAllocator {
public:
    VirtualMemoryAllocator();
    VirtualMemoryAllocator(void* start, uint64_t pageCount);
    ~VirtualMemoryAllocator();

    void* AllocatePage();
    void* AllocatePages(uint64_t pageCount);

    void* AllocatePage(void* address);
    void* AllocatePages(void* address, uint64_t pageCount);

    void FreePage(void* address);
    void FreePages(void* address, uint64_t pageCount);

    bool ReservePages(void* address, uint64_t pageCount);
    void UnreservePages(void* address, uint64_t pageCount);

    void AddRegion(void* address, uint64_t pageCount);

private:
    void Verify();

private:
    struct PageRegionData {
        uint64_t pageCount : 62;
        uint64_t free : 1;
        uint64_t reserved : 1;
    } __attribute__((packed));

    AVLTree::LockableAVLTree<uint64_t, LinkedList::Node*> m_freeSizeTree;
    AVLTree::LockableAVLTree<void*, uint64_t> m_allAddressTree;

    uint64_t m_freePages;
    uint64_t m_usedPages;
    uint64_t m_reservedPages;

    spinlock_t m_lock;
};

extern VirtualMemoryAllocator* g_GlobalVMA;
extern VirtualMemoryAllocator* g_KVMA;
extern VirtualMemoryAllocator* g_KExecVMA;

#endif /* _VIRTUAL_MEMORY_ALLOCATOR_HPP */