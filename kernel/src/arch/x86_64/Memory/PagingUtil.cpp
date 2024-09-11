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

#include "PagingUtil.hpp"
#include "PageTables.hpp"
#include "PagingInit.hpp"

#include <stdint.h>
#include <util.h>

#include <Memory/PagingUtil.hpp>

void x86_64_MapRegionWithLargestPages(x86_64_Level4Table* Table, uint64_t physical_base, uint64_t length, uint32_t flags) {
    x86_64_MapRegionWithLargestPages(Table, to_HHDM(physical_base), physical_base, length, flags);
}

void x86_64_MapRegionWithLargestPages(x86_64_Level4Table* Table, uint64_t virtual_base, uint64_t physical_base, uint64_t length, uint32_t flags) {
    virtual_base = ALIGN_DOWN(virtual_base, PAGE_SIZE);
    physical_base = ALIGN_DOWN(physical_base, PAGE_SIZE);
    length = ALIGN_UP(length, PAGE_SIZE);

    uint32_t largeFlags = (flags & 0x0FFF'0F7F) | ((flags & (1<<7)) << (12-7)); // move bit 7 to bit 12

    bool can_use_2MiB = x86_64_is2MiBPagesSupported() && (ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base) == (ALIGN_UP(physical_base, LARGE_PAGE_SIZE) - physical_base) && length >= LARGE_PAGE_SIZE;
    bool can_use_1GiB = x86_64_is1GiBPagesSupported() && (ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base) == (ALIGN_UP(physical_base, HUGE_PAGE_SIZE) - physical_base) && length >= HUGE_PAGE_SIZE;
    
    if (can_use_2MiB)
        can_use_2MiB = can_use_2MiB && (ALIGN_DOWN(length, LARGE_PAGE_SIZE) - (ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base)) > 0;

    if (can_use_1GiB)
        can_use_1GiB = can_use_1GiB && (ALIGN_DOWN(length, HUGE_PAGE_SIZE) - (ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base)) > 0;

    if (!can_use_2MiB && !can_use_1GiB) {
        for (uint64_t i = 0; i < length; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);
    }
    else if (!can_use_1GiB && can_use_2MiB) {
        // first map with 4KiB pages until we reach the 2MiB boundary
        for (uint64_t i = 0; i < ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);

        // then map with 2MiB pages while we can
        for (uint64_t i = ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base; i < ALIGN_DOWN((virtual_base + length), LARGE_PAGE_SIZE) - virtual_base; i += LARGE_PAGE_SIZE)
            x86_64_Map2MiBPage(Table, virtual_base + i, physical_base + i, largeFlags);

        // then map the rest with 4KiB pages
        for (uint64_t i = ALIGN_DOWN((virtual_base + length), LARGE_PAGE_SIZE) - virtual_base; i < length; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);
    }
    else if (can_use_1GiB && !can_use_2MiB) {
        // first map with 4KiB pages until we reach the 1GiB boundary
        for (uint64_t i = 0; i < ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);

        // then map with 1GiB pages while we can
        for (uint64_t i = ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base; i < ALIGN_DOWN((virtual_base + length), HUGE_PAGE_SIZE) - virtual_base; i += HUGE_PAGE_SIZE)
            x86_64_Map1GiBPage(Table, virtual_base + i, physical_base + i, flags);

        // then map the rest with 4KiB pages
        for (uint64_t i = ALIGN_DOWN((virtual_base + length), HUGE_PAGE_SIZE) - virtual_base; i < length; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);
    }
    else if (can_use_1GiB && can_use_2MiB) {
        // first map with 4KiB pages until we reach the 2MiB boundary
        for (uint64_t i = 0; i < ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);

        // then map with 2MiB pages until we reach the 1GiB boundary
        for (uint64_t i = ALIGN_UP(virtual_base, LARGE_PAGE_SIZE) - virtual_base; i < ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base; i += LARGE_PAGE_SIZE)
            x86_64_Map2MiBPage(Table, virtual_base + i, physical_base + i, largeFlags);

        // then map with 1GiB pages while we can
        for (uint64_t i = ALIGN_UP(virtual_base, HUGE_PAGE_SIZE) - virtual_base; i < ALIGN_DOWN((virtual_base + length), HUGE_PAGE_SIZE) - virtual_base; i += HUGE_PAGE_SIZE)
            x86_64_Map1GiBPage(Table, virtual_base + i, physical_base + i, flags);

        // then map with 2MiB pages while we can
        for (uint64_t i = ALIGN_DOWN((virtual_base + length), HUGE_PAGE_SIZE) - virtual_base; i < ALIGN_DOWN((virtual_base + length), LARGE_PAGE_SIZE) - virtual_base; i += LARGE_PAGE_SIZE)
            x86_64_Map2MiBPage(Table, virtual_base + i, physical_base + i, largeFlags);

        // then map the rest with 4KiB pages
        for (uint64_t i = ALIGN_DOWN((virtual_base + length), LARGE_PAGE_SIZE) - virtual_base; i < length; i += PAGE_SIZE)
            x86_64_MapPage(Table, virtual_base + i, physical_base + i, flags);
    }
}
