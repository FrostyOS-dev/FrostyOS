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

#ifndef _KERNEL_HAL_HPP
#define _KERNEL_HAL_HPP

#include <stdint.h>

#include <Memory/MemoryMap.hpp>

#ifdef __x86_64__
#include <arch/x86_64/interrupts/IRQ.hpp>

#include <arch/x86_64/Memory/PagingUtil.hpp>

#include <arch/x86_64/ArchDefs.h>
#include <arch/x86_64/Panic.hpp>

typedef x86_64_PagingMode PagingMode;
typedef x86_64_Registers CPU_Registers;
typedef uint32_t (*GSIHandler_t)(void* ctx);
#endif

void HAL_EarlyInit(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical, void* RSDP);
void HAL_Stage2();

void* HAL_RegisterIntHandler(uint32_t GSI, GSIHandler_t handler, void* ctx);
bool HAL_RemoveIntHandler(void* data);

#endif /* _KERNEL_HAL_HPP */