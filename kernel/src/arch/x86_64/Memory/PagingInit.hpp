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

#ifndef _x86_64_PAGING_INIT_HPP
#define _x86_64_PAGING_INIT_HPP

#include "PagingUtil.hpp"

#include <stdint.h>

#include <Memory/MemoryMap.hpp>

/*
Virtual Memory Regions (4 level paging):
0x0000000000000000 - 0x0000000000000FFF (   4KiB): Reserved
0x0000000000001000 - 0x00007FFFFFFFFFFF (~128TiB): User space
0x0000800000000000 - 0xFFFF7FFFFFFFFFFF ( ~16EiB): Reserved non-canonical space
0xFFFF800000000000 - 0xFFFFBFFFFFFFFFFF (  64TiB): HHDM
0xFFFFC00000000000 - 0xFFFFEFFFFFFFFFFF (  48TiB): Allocatable kernel space
0xFFFFF00000000000 - 0xFFFFFFFF7FFFFFFF ( ~16TiB): Reserved kernel space
0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF (   2GiB): Kernel executable loading space

Virtual Memory Regions (5 level paging):
0x0000000000000000 - 0x0000'0000'0000'0FFF (   4KiB): Reserved
0x0000000000001000 - 0x00FF'FFFF'FFFF'FFFF (  ~1PiB): User space
0x0100000000000000 - 0xFEFF'FFFF'FFFF'FFFF ( ~64PiB): Reserved non-canonical space
0xFF00000000000000 - 0xFF7F'FFFF'FFFF'FFFF ( 128TiB): HHDM
0xFF80000000000000 - 0xFFBF'FFFF'FFFF'FFFF (  64TiB): Allocatable kernel space
0xFFC0000000000000 - 0xFFFF'FFFF'7FFF'FFFF ( ~64TiB): Reserved kernel space
0xFFFFFFFF80000000 - 0xFFFF'FFFF'FFFF'FFFF (   2GiB): Kernel executable loading space
*/

void x86_64_InitPaging(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, x86_64_PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical);

extern void* g_KernelRootPageTable;

#endif /* _x86_64_PAGING_INIT_HPP */