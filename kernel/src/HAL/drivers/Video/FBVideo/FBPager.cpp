/*
Copyright (©) 2024-2026  Frosty515

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

#include "FBDisplay.hpp"
#include "FBPager.hpp"

#include <stdint.h>
#include <util.h>

#include <Graphics/Framebuffer.hpp>

#include <Memory/Pager.hpp>
#include <Memory/PagingUtil.hpp>
#include <Memory/VMM.hpp>

namespace Video {

    FBDisplayPager::FBDisplayPager() {

    }

    FBDisplayPager::~FBDisplayPager() {

    }

    void* FBDisplayPager::AllocatePage() {
        return nullptr; // not a valid operation
    }

    bool FBDisplayPager::GetPage(VMM::MemoryObject* obj, uint64_t offset, VMM::Page** outPage, bool write) {
        (void)write;
        if ((offset >> PAGE_SIZE_SHIFT) + 1 > obj->size)
            return false;

        FBDisplay* display = static_cast<FBDisplay*>(obj->pagerData);
        if (display == nullptr)
            return false;

        FrameBuffer* fb = display->GetFrameBuffer();
        if (fb == nullptr)
            return false;

        VMM::Page* page = obj->pages.Find(offset);
        if (page == nullptr) {
            page = static_cast<VMM::Page*>(kcalloc_vmm(1, sizeof(VMM::Page)));
            if (page == nullptr)
                return false;
            page->physAddr = from_HHDM((uint64_t)fb->BaseAddress + offset);
            page->protection = display->GetDefaultProt();
            obj->pages.Insert(offset, page);
        }
        *outPage = page;
        return true;
    }

    void FBDisplayPager::FreePage(void* page) {
        (void)page;
    }

}
