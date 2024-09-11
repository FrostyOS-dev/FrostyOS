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

#ifndef _x86_64_PAGING_UTIL_HPP
#define _x86_64_PAGING_UTIL_HPP

#include <stdint.h>

#include "PageTables.hpp"

extern "C" {
    bool x86_64_EnsureNX();

    bool x86_64_Ensure2MBPages();
    bool x86_64_Ensure1GBPages();
}

void x86_64_MapRegionWithLargestPages(x86_64_Level4Table* Table, uint64_t physical_base, uint64_t length, uint32_t flags);
void x86_64_MapRegionWithLargestPages(x86_64_Level4Table* Table, uint64_t virtual_base, uint64_t physical_base, uint64_t length, uint32_t flags);

#endif /* _x86_64_PAGING_UTIL_HPP */