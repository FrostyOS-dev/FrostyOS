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

#ifndef _TEMPFS_PAGER_HPP
#define _TEMPFS_PAGER_HPP

#include <stdint.h>

#include <Memory/Pager.hpp>

namespace FS {
    class TempFSVNode;

    class TempFSPager : public VMM::DefaultPager {
    public:
        TempFSPager();
        virtual ~TempFSPager() override;

        virtual void* AllocatePage() override;
        virtual bool GetPage(VMM::MemoryObject* obj, uint64_t offset, VMM::Page** outPage, bool write) override; // obj is assumed to already be locked
        virtual void FreePage(void* page) override;
    };
}

#endif /* _TEMPFS_PAGER_HPP */