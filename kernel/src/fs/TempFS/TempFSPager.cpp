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

#include "TempFS.hpp"
#include "TempFSPager.hpp"

#include "../VFS.hpp"

#include <stdint.h>
#include <spinlock.h>
#include <string.h>
#include <util.h>

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
        TempFSVNode* vnode = static_cast<TempFSVNode*>(obj->pagerData);
        if (vnode == nullptr)
            return false;
        VMM::Page* page = obj->pages.Find(offset);
        if (page == nullptr) {
            page = static_cast<VMM::Page*>(kcalloc_vmm(1, sizeof(VMM::Page)));
            if (page == nullptr)
                return false;
            page->physAddr = (uint64_t)AllocatePage();
            page->protection = vnode->GetDefaultProt();
            obj->pages.Insert(offset, page);
        }
        *outPage = page;
        return true;
    }

    void TempFSPager::FreePage(void* page) {
        g_PMM->FreePage(page);
    }


}