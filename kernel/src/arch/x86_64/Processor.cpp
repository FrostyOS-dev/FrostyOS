/*
Copyright (©) 2025  Frosty515

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

#include "Processor.hpp"

#include "GDT.hpp"
#include "PIT.hpp"

#include "interrupts/IDT.hpp"
#include "interrupts/IRQ.hpp"

#include "Memory/PagingInit.hpp"

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

x86_64_Processor g_x86_64_BSP(true);
Processor* g_BSP = &g_x86_64_BSP;

x86_64_Processor::x86_64_Processor(bool BSP) {
    m_BSP = BSP; // member of parent class
}

x86_64_Processor::~x86_64_Processor() {

}

void x86_64_Processor::Init() {
    if (m_BSP)
        PANIC("BSP called Init() without arguments");

    PANIC("AP Init() not implemented");
}

void x86_64_Processor::Init(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) {
    if (!m_BSP)
        PANIC("AP called Init() with arguments");

    x86_64_InitGDT();
    x86_64_InitIDT();
    x86_64_IRQ_Init();
    x86_64_PIT_Init();
    x86_64_InitPaging(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
}
