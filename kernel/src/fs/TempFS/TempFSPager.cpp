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

#include "TempFSPager.hpp"

#include <stdint.h>
#include <spinlock.h>
#include <string.h>
#include <util.h>

#include <DataStructures/AVLTree.hpp>

#include <Memory/Pager.hpp>
#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>
#include <Memory/VMM.hpp>

namespace FS {

    TempFSPager::TempFSPager() {

    }

    TempFSPager::~TempFSPager() {

    }

    void* TempFSPager::AllocatePage() {
        void* phys = g_PMM->AllocatePage();
        memset(to_HHDM(phys), 0, PAGE_SIZE);
        return phys;
    }

    bool TempFSPager::GetPage(VMM::MemoryObject* obj, uint64_t offset, VMM::Page** outPage, bool write) {
        (void)write;
        if ((offset >> PAGE_SIZE_SHIFT) + 1 > obj->size)
            return false;
        VMM::Page* page = obj->pages.Find(offset);
        if (page == nullptr) {
            page = static_cast<VMM::Page*>(kcalloc_vmm(1, sizeof(VMM::Page)));
            if (page == nullptr)
                return false;
            page->physAddr = (uint64_t)AllocatePage();
            page->protection = VMM::Protection::READ_WRITE;
            obj->pages.Insert(offset, page);
        }
        *outPage = page;
        return true;
    }

    void TempFSPager::FreePage(void* page) {
        g_PMM->FreePage(page);
    }

    void TempFSPager::ZeroRange(VMM::MemoryObject* obj, uint64_t from, uint64_t to) {
        if (from >= to)
            return;
        spinlock_acquire(&obj->lock);
        for (AVLTree::wAVLTreeNode* node = obj->pages.FindNodeOrHigher(ALIGN_DOWN(from, PAGE_SIZE)); node != nullptr && node->key < to; node = obj->pages.NextNode(node)) {
            VMM::Page* page = reinterpret_cast<VMM::Page*>(node->value);
            if (page->physAddr == 0)
                continue;
            uint64_t s = from > node->key ? from - node->key : 0;
            uint64_t e = (to - node->key) < PAGE_SIZE ? to - node->key : PAGE_SIZE;
            memset((uint8_t*)(to_HHDM(page->physAddr) + s), 0, e - s);
        }
        spinlock_release(&obj->lock);
    }

    void TempFSPager::DiscardPages(VMM::MemoryObject* obj, uint64_t fromOffset) {
        while (true) {
            spinlock_acquire(&obj->lock);
            AVLTree::wAVLTreeNode* node = obj->pages.FindNodeOrHigher(fromOffset);
            if (node == nullptr) {
                spinlock_release(&obj->lock);
                return;
            }
            VMM::Page* page = reinterpret_cast<VMM::Page*>(node->value);
            obj->pages.RemoveNode(node);
            spinlock_release(&obj->lock);

            if (page->physAddr != 0)
                FreePage((void*)page->physAddr);
            kfree_vmm(page);
        }
    }
}