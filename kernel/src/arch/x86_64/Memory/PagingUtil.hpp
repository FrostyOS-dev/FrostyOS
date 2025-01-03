/*
Copyright (©) 2024-2025  Frosty515

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

#ifndef _x86_64_PAGING_UTIL_HPP
#define _x86_64_PAGING_UTIL_HPP

#include <stdint.h>

enum class x86_64_PagingMode {
    _4LVL,
    _5LVL
};

// Assembly functions, not intended to be called directly in most cases

extern "C" {

bool x86_64_Internal_Is5LevelPagingSupported();

bool x86_64_EnsureNXSupport();
bool x86_64_Ensure2MiBPageSupport();
bool x86_64_Ensure1GiBPageSupport();

// following functions can be called directly
void x86_64_LoadCR3(uint64_t cr3);
void x86_64_InvalidatePage(uint64_t virtualAddress);
void x86_64_FlushTLB();

}

// C++ functions, intended to be called directly

bool x86_64_Is5LevelPagingSupported();
bool x86_64_Is2MiBPageSupported();
bool x86_64_Is1GiBPageSupported();

void x86_64_MapRegionWithLargestPages(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint64_t size, uint32_t flags);

uint32_t x86_64_ConvertToLargePageFlags(uint32_t flags);

void x86_64_InvalidatePages(uint64_t virtualAddress, uint64_t size);

#endif /* _x86_64_PAGING_UTIL_HPP */