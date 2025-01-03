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

#include "PageTables.hpp"
#include "PagingUtil.hpp"

#include <stddef.h>
#include <stdio.h>
#include <util.h>

#define FULL_FLUSH_THRESHOLD 0x1000 // 2 whole level 1 tables

bool g_x86_64_5LevelPagingSupportChecked = false;
bool g_x86_64_2MiBPageSupportChecked = false;
bool g_x86_64_1GiBPageSupportChecked = false;

bool g_x86_64_Is5LevelPagingSupported = false;
bool g_x86_64_Is2MiBPageSupported = false;
bool g_x86_64_Is1GiBPageSupported = false;

bool x86_64_Is5LevelPagingSupported() {
    if (!g_x86_64_5LevelPagingSupportChecked) {
        g_x86_64_Is5LevelPagingSupported = x86_64_Internal_Is5LevelPagingSupported();
        g_x86_64_5LevelPagingSupportChecked = true;
    }
    return g_x86_64_Is5LevelPagingSupported;
}

bool x86_64_Is2MiBPageSupported() {
    if (!g_x86_64_2MiBPageSupportChecked) {
        g_x86_64_Is2MiBPageSupported = x86_64_Ensure2MiBPageSupport();
        g_x86_64_2MiBPageSupportChecked = true;
    }
    return g_x86_64_Is2MiBPageSupported;
}

bool x86_64_Is1GiBPageSupported() {
    if (!g_x86_64_1GiBPageSupportChecked) {
        g_x86_64_Is1GiBPageSupported = x86_64_Ensure1GiBPageSupport();
        g_x86_64_1GiBPageSupportChecked = true;
    }
    return g_x86_64_Is1GiBPageSupported;
}

void x86_64_MapRegionWithLargestPages(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint64_t size, uint32_t flags) {
    uint32_t largeFlags = x86_64_ConvertToLargePageFlags(flags);
    
    // first test if no large pages are supported, or the size is less than 2MiB, or 2MiB pages are not supported, and the size is less than 1GiB
    if ((!x86_64_Is2MiBPageSupported() && !x86_64_Is1GiBPageSupported()) || size < LARGE_PAGE_SIZE || (size < HUGE_PAGE_SIZE && !x86_64_Is2MiBPageSupported())) {
        size_t PageCount = DIV_ROUNDUP(size, PAGE_SIZE);
        for (size_t i = 0; i < PageCount; i++)
            x86_64_MapPage(pageTable, virtualAddress + i * PAGE_SIZE, physicalAddress + i * PAGE_SIZE, flags);
        return;
    }

    // next see if we have 2MiB pages, but not 1GiB pages, or the size is less than 1GiB
    if ((x86_64_Is2MiBPageSupported() && !x86_64_Is1GiBPageSupported()) || size < HUGE_PAGE_SIZE) {
        // map until 2MiB boundary
        size_t StartOf2MiBAlignment = ALIGN_UP(physicalAddress, LARGE_PAGE_SIZE);
        for (size_t i = physicalAddress; i < StartOf2MiBAlignment; i += PAGE_SIZE)
            x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);

        // map 2MiB pages until end of alignment
        size_t EndOf2MiBAlignment = ALIGN_DOWN((physicalAddress + size), LARGE_PAGE_SIZE);
        for (size_t i = StartOf2MiBAlignment; i < EndOf2MiBAlignment; i += LARGE_PAGE_SIZE)
            x86_64_Map2MiBPage(pageTable, i - physicalAddress + virtualAddress, i, largeFlags);

        // map remaining pages
        for (size_t i = EndOf2MiBAlignment; i < (physicalAddress + size); i += PAGE_SIZE)
            x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);

        return;
    }

    // next see if we have the rare case of 1GiB pages, but not 2MiB pages
    if (!x86_64_Is2MiBPageSupported() && x86_64_Is1GiBPageSupported()) {
        // map until 1GiB boundary
        size_t StartOf1GiBAlignment = ALIGN_UP(physicalAddress, HUGE_PAGE_SIZE);
        for (size_t i = physicalAddress; i < StartOf1GiBAlignment; i += PAGE_SIZE)
            x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);

        // map 1GiB pages until end of alignment
        size_t EndOf1GiBAlignment = ALIGN_DOWN((physicalAddress + size), HUGE_PAGE_SIZE);
        for (size_t i = StartOf1GiBAlignment; i < EndOf1GiBAlignment; i += HUGE_PAGE_SIZE)
            x86_64_Map1GiBPage(pageTable, i - physicalAddress + virtualAddress, i, largeFlags);

        // map remaining pages
        for (size_t i = EndOf1GiBAlignment; i < (physicalAddress + size); i += PAGE_SIZE)
            x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);
        
        return;
    }

    // finally, we have both 2MiB and 1GiB pages
    // map until 2MiB boundary
    size_t StartOf2MiBAlignment = ALIGN_UP(physicalAddress, LARGE_PAGE_SIZE);
    for (size_t i = physicalAddress; i < StartOf2MiBAlignment; i += PAGE_SIZE)
        x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);

    // map 2MiB pages until 1GiB boundary
    size_t StartOf1GiBAlignment = ALIGN_UP(physicalAddress, HUGE_PAGE_SIZE);
    for (size_t i = StartOf2MiBAlignment; i < StartOf1GiBAlignment; i += LARGE_PAGE_SIZE)
        x86_64_Map2MiBPage(pageTable, i - physicalAddress + virtualAddress, i, largeFlags);

    // map 1GiB pages until end of alignment
    size_t EndOf1GiBAlignment = ALIGN_DOWN((physicalAddress + size), HUGE_PAGE_SIZE);
    for (size_t i = StartOf1GiBAlignment; i < EndOf1GiBAlignment; i += HUGE_PAGE_SIZE)
        x86_64_Map1GiBPage(pageTable, i - physicalAddress + virtualAddress, i, largeFlags);

    // map 2MiB pages until end of alignment
    size_t EndOf2MiBAlignment = ALIGN_DOWN((physicalAddress + size), LARGE_PAGE_SIZE);
    for (size_t i = EndOf1GiBAlignment; i < EndOf2MiBAlignment; i += LARGE_PAGE_SIZE)
        x86_64_Map2MiBPage(pageTable, i - physicalAddress + virtualAddress, i, largeFlags);

    // map remaining pages
    for (size_t i = EndOf2MiBAlignment; i < (physicalAddress + size); i += PAGE_SIZE)
        x86_64_MapPage(pageTable, i - physicalAddress + virtualAddress, i, flags);
}

uint32_t x86_64_ConvertToLargePageFlags(uint32_t flags) {
    // just need to move the PAT bit from bit 7 to bit 12
    return (flags & 0x0800'0F7F) | ((flags & 0x0000'0080) << (12 - 7));
}

void x86_64_InvalidatePages(uint64_t virtualAddress, uint64_t size) {
    uint64_t pageCount = DIV_ROUNDUP(size, PAGE_SIZE);
    
    if (pageCount >= FULL_FLUSH_THRESHOLD) {
        x86_64_FlushTLB();
        return;
    }

    for (uint64_t i = 0; i < pageCount; i++)
        x86_64_InvalidatePage(virtualAddress + i * PAGE_SIZE);
}
