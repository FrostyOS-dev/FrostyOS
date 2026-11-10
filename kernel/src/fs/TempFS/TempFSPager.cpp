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
            obj->pages.Insert(page, offset);
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
        obj->lock.Lock();
        obj->pages.ForEachFrom([&](uint64_t offset, VMM::Page* p) -> bool {
            if (offset >= to)
                return false;
            if (p->physAddr == 0)
                return true;
            uint64_t s = from > offset ? from - offset : 0;
            uint64_t e = (to - offset) < PAGE_SIZE ? to - offset : PAGE_SIZE;
            memset((uint8_t*)(to_HHDM(p->physAddr) + s), 0, e - s);
            return true;
        }, ALIGN_DOWN(from, PAGE_SIZE));
        obj->lock.Unlock();
    }

    void TempFSPager::DiscardPages(VMM::MemoryObject* obj, uint64_t fromOffset) {
        while (true) {
            obj->lock.Lock();
            VMM::Page* page = obj->pages.FindGE(fromOffset);
            if (page == nullptr) {
                obj->lock.Unlock();
                return;
            }
            obj->pages.Remove(page);
            obj->lock.Unlock();

            if (page->physAddr != 0)
                FreePage((void*)page->physAddr);
            kfree_vmm(page);
        }
    }
}