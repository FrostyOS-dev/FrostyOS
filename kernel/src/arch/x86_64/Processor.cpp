/*
Copyright (©) 2025-2026  Frosty515

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

#include "interrupts/APIC/IOAPIC.hpp"

#include "Memory/PagingInit.hpp"

#include "Scheduling/TaskUtil.hpp"

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>

x86_64_Processor g_x86_64_BSP(true);
Processor* g_BSP = &g_x86_64_BSP;

x86_64_Processor::x86_64_Processor(bool BSP) : m_IRQData(nullptr), m_LAPIC(nullptr) {
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
    x86_64_IRQ_EarlyInit();
    x86_64_InitPaging(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
    g_KProcess->SetVMM(VMM::g_KVMM);
    
    Scheduler::InitBSPState();
    x86_64_SetGSBases(0, (uint64_t)&Scheduler::g_BSPState);

    x86_64_IRQ_FullInit();
}

void x86_64_Processor::InitTime() {
    x86_64_PIT_Init();
    x86_64_UnmaskGSI(x86_64_GetGSIFromSource(0));
}

void x86_64_Processor::SetIRQData(x86_64_ProcessorIRQData* data) {
    m_IRQData = data;
}

x86_64_ProcessorIRQData* x86_64_Processor::GetIRQData() {
    return m_IRQData;
}

void x86_64_Processor::SetLAPIC(x86_64_LAPIC* lapic) {
    m_LAPIC = lapic;
}

x86_64_LAPIC* x86_64_Processor::GetLAPIC() const {
    return m_LAPIC;
}
