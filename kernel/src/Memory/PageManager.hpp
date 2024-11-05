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

#ifndef _PAGE_MANAGER_HPP
#define _PAGE_MANAGER_HPP

#include <stdint.h>
#include <spinlock.h>

#include <Data-structures/AVLTree.hpp>

#include "PageTable.hpp"
#include "VirtualMemoryAllocator.hpp"
#include "VMRegion.hpp"

class PageManager {
public:
    PageManager();
    PageManager(PageTable* table, VirtualMemoryAllocator* vma, bool user = false);
    ~PageManager();

    void Initialise(PageTable* table, VirtualMemoryAllocator* vma, bool user = false);

    void* AllocatePages(uint64_t pageCount, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);
    void* AllocatePage(PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);
    void FreePages(void* address, uint64_t pageCount = 0);
    void FreePage(void* address);
    void* AllocatePages(void* address, uint64_t pageCount, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);
    void* AllocatePage(void* address, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);

    void* MapPages(void* virtualAddress, void* physicalAddress, uint64_t pageCount, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT); // virtual address is nullptr for automatic allocation
    void* MapPage(void* virtualAddress, void* physicalAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);
    void UnmapPages(void* virtualAddress, uint64_t pageCount = 0);
    void UnmapPage(void* virtualAddress);

    void RemapPages(void* virtualAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT);

    void PrintRegions();

private:
    void Verify();

private:
    PageTable* m_table;
    VirtualMemoryAllocator* m_vma;
    bool m_user;
    spinlock_t m_lock;
    AVLTree::LockableAVLTree<void*, VMRegion*> m_regions;
};

extern PageManager* g_KPM;

#endif /* _PAGE_MANAGER_HPP */