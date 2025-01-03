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

#include "HAL.hpp"

#ifdef __x86_64__
#include <arch/x86_64/GDT.hpp>

#include <arch/x86_64/interrupts/IDT.hpp>

#include <arch/x86_64/Memory/PagingInit.hpp>

void HAL_EarlyInit(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) {
    x86_64_InitGDT();
    x86_64_InitIDT();
    x86_64_InitPaging(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
}

#else
#error "HAL: Unsupported architecture"
#endif
