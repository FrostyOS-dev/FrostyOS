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

#ifndef _x86_64_PAGING_INIT_HPP
#define _x86_64_PAGING_INIT_HPP

#include <Memory/MemoryMap.hpp>

void x86_64_InitPaging(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, void* fb_base, uint64_t fb_size, uint64_t kernel_virtual, uint64_t kernel_physical);

bool x86_64_is2MiBPagesSupported();
bool x86_64_is1GiBPagesSupported();

void MapKernel(uint64_t kernel_virtual, uint64_t kernel_physical);

#endif /* _x86_64_PAGING_INIT_HPP */