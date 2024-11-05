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

#include "ACPI/Init.hpp"

#ifdef __x86_64__
#include <arch/x86_64/GDT.hpp>
#include <arch/x86_64/Processor.hpp>

#include <arch/x86_64/interrupts/IDT.hpp>
#include <arch/x86_64/interrupts/PIC.hpp>

#include <arch/x86_64/Memory/PagingInit.hpp>

Processor BSP(true);

void HAL_EarlyInit(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, void* fb_base, uint64_t fb_size, uint64_t kernel_virtual, uint64_t kernel_physical, void* RSDP) {
    g_BSP = &BSP;
    g_BSP->Init(nullptr);
    x86_64_InitPaging(memoryMap, memoryMapEntryCount, fb_base, fb_size, kernel_virtual, kernel_physical);

    // ACPI::EarlyInit(RSDP);
}

void HAL_Stage2() {
    ACPI::BaseInit();
    // do timer and IRQ stuff

    ACPI::FullInit();
}

#endif