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

#ifndef _VM_REGION_HPP
#define _VM_REGION_HPP

#include <stddef.h>
#include <stdint.h>

#include "PageTable.hpp"


struct VMRegionFlags {
    PagePermission Permission;
    PageCache Cache;
    bool Pageable : 1;
    bool PagedIn : 1;
    bool Shared : 1; // 0 = private, 1 = shared
    uint8_t Reserved : 5;
};

#define DEFAULT_VMREGION_FLAGS { PagePermission::READ_WRITE, PageCache::DEFAULT, true, true, false, 0 }

class VMRegion {
public:
    VMRegion(void* start, void* end, VMRegionFlags flags = DEFAULT_VMREGION_FLAGS);
    ~VMRegion();

    void* GetStart() const;
    void* GetEnd() const;
    size_t GetSize() const;
    VMRegionFlags GetFlags() const;

    void SetFlags(VMRegionFlags flags);

    void ExpandLeft(size_t size);
    void ExpandRight(size_t size);

    bool Contains(void* addr, size_t size = 0) const;
    bool Overlaps(const VMRegion& other) const;

private:
    void* m_start;
    void* m_end;
    VMRegionFlags m_flags;
};

#endif /* _VM_REGION_HPP */