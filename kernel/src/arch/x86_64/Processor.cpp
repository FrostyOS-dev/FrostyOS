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

#include "CPUID.h"
#include "GDT.hpp"
#include "PIT.hpp"
#include "TSC.hpp"

#include "interrupts/IDT.hpp"
#include "interrupts/IRQ.hpp"

#include "interrupts/APIC/IOAPIC.hpp"

#include "Memory/PagingInit.hpp"

#include "Scheduling/TaskUtil.hpp"
#include "util.h"

#include <stdint.h>
#include <string.h>
#include <spinlock.h>

#include <kernel.hpp>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>

x86_64_Processor g_x86_64_BSP(true);
Processor* g_BSP = &g_x86_64_BSP;

x86_64_Processor::x86_64_Processor(bool BSP) : apLock(SPINLOCK_LOCKED_VALUE), m_IRQData(nullptr), m_LAPIC(nullptr), m_TSCAvailable(false) {
    m_BSP = BSP; // member of parent class
}

x86_64_Processor::~x86_64_Processor() {

}

[[noreturn]] void x86_64_Processor::Init(uint64_t stackTop) {
    if (m_BSP)
        PANIC("BSP called Init() without arguments");

    memset(&m_TSS, 0, sizeof(x86_64_TSS));
    x86_64_InitGDT(m_GDT, &m_TSS);

    Scheduler::ProcessorState* state = Scheduler::InitNewProcessor(this);
    x86_64_SetGSBases(0, (uint64_t)state);

    FillCPUInfo();

    m_LAPIC->Init(true);

    m_TSCAvailable = x86_64_TSCInit(this);

    state->kernelStack = reinterpret_cast<void*>(stackTop);
    InitTSS(state);

    spinlock_release(&apLock);

    Scheduler::WaitForStart(state);

    if (!m_LAPIC->InitTimer())
        PANIC("AP LAPIC timer init failed");

    Thread* thread = new Thread({Kernel_Idle, nullptr}, g_KLowestPriorityProcess);
    thread->SetStack((uint64_t)VMM::g_KVMM->AllocatePages(KERNEL_STACK_SIZE / PAGE_SIZE, VMM::Protection::READ_WRITE, true) + KERNEL_STACK_SIZE);
    g_KLowestPriorityProcess->AddThread(thread);

    Scheduler::ScheduleThread(thread, state);

    Scheduler::Start(true);
}

void x86_64_Processor::Init(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) {
    if (!m_BSP)
        PANIC("AP called Init() with arguments");

    memset(&m_TSS, 0, sizeof(x86_64_TSS));
    x86_64_InitGDT(m_GDT, &m_TSS);
    x86_64_InitIDT();

    FillCPUInfo();

    x86_64_IRQ_EarlyInit();
    x86_64_InitPaging(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
    g_KProcess->SetVMM(VMM::g_KVMM);
    g_KLowestPriorityProcess->SetVMM(VMM::g_KVMM);
    
    Scheduler::InitBSPState();
    x86_64_SetGSBases(0, (uint64_t)&Scheduler::g_BSPState);

    uint64_t kernelStack = (uint64_t)VMM::g_KVMM->AllocatePages(KERNEL_STACK_SIZE / PAGE_SIZE, VMM::Protection::READ_WRITE, true) + KERNEL_STACK_SIZE;
    Scheduler::g_BSPState.kernelStack = (void*)kernelStack;
    InitTSS(&Scheduler::g_BSPState);

    x86_64_IRQ_FullInit();
}

void x86_64_Processor::InitBSPLate() {
    if (!m_BSP)
        return;

    x86_64_IDT_SetISTS();
}

void x86_64_Processor::InitTSS(Scheduler::ProcessorState* state) {
    m_TSS.RSP[0] = (uint64_t)state->kernelStack;
    m_TSS.IST[0] = (uint64_t)VMM::g_KVMM->AllocatePages(KERNEL_STACK_SIZE / PAGE_SIZE, VMM::Protection::READ_WRITE, true) + KERNEL_STACK_SIZE;
    m_TSS.IST[1] = (uint64_t)VMM::g_KVMM->AllocatePages(KERNEL_STACK_SIZE / PAGE_SIZE, VMM::Protection::READ_WRITE, true) + KERNEL_STACK_SIZE;
    if (state->kernelStack == 0 || m_TSS.IST[0] == 0 || m_TSS.IST[1] == 0)
        PANIC("Failed to allocate stack for TSS");

    x86_64_LoadTSS(x86_64_TSS_SEGMENT);
}

void x86_64_Processor::InitTime() {
    m_TSCAvailable = x86_64_TSCInit(this);
    if (!m_LAPIC->InitTimer() && m_BSP) {
        dbgprintf("Warning: Local APIC timer is unavailable, falling back to the PIT\n");
        
        x86_64_PIT_Init();
        x86_64_UnmaskGSI(x86_64_GetGSIFromSource(0));
    }
}

void x86_64_Processor::FillCPUInfo() {
    // start with max CPUID leaf and vendor string
    x86_64_CPUIDResult result = x86_64_CPUID(0, 0);
    m_info.maxCPUID = result.EAX;
    
    memcpy(&m_info.vendorStr[0], &result.EBX, 4);
    memcpy(&m_info.vendorStr[4], &result.EDX, 4);
    memcpy(&m_info.vendorStr[8], &result.ECX, 4);

#define VENDOR_STR_CMP(a) strncmp(m_info.vendorStr, a, 12) == 0

    if (VENDOR_STR_CMP("GenuineIntel"))
        m_info.vendor = x86_VENDOR_INTEL;
    else if (VENDOR_STR_CMP("CyrixInstead"))
        m_info.vendor = x86_VENDOR_CYRIX;
    else if (VENDOR_STR_CMP("AuthenticAMD"))
        m_info.vendor = x86_VENDOR_AMD;
    else if (VENDOR_STR_CMP("UMC UMC UMC "))
        m_info.vendor = x86_VENDOR_UMC;
    else if (VENDOR_STR_CMP("CentaurHauls"))
        m_info.vendor = x86_VENDOR_CENTAUR;
    else if (VENDOR_STR_CMP("GenuineTMx86"))
        m_info.vendor = x86_VENDOR_TRANSMETA;
    else if (VENDOR_STR_CMP("Geode by NSC"))
        m_info.vendor = x86_VENDOR_NSC;
    else if (VENDOR_STR_CMP("HygonGenuine"))
        m_info.vendor = x86_VENDOR_HYGON;
    else if (VENDOR_STR_CMP("  Shanghai  "))
        m_info.vendor = x86_VENDOR_ZHAOXIN;
    else if (VENDOR_STR_CMP("Vortex86 SoC"))
        m_info.vendor = x86_VENDOR_VORTEX;
    else
        m_info.vendor = x86_VENDOR_UNKNOWN;

#undef VENDOR_STR_CMP

    // next get max extended CPUID leaf and model string
    result = x86_64_CPUID(0x80000000, 0);
    m_info.maxExtendedCPUID = result.EAX;

    for (int i = 0; i < 3; i++) {
        result = x86_64_CPUID(0x80000002 + i, 0);

        memcpy(&m_info.modelStr[i * 16], &result.EAX, 4);
        memcpy(&m_info.modelStr[i * 16 + 4], &result.EBX, 4);
        memcpy(&m_info.modelStr[i * 16 + 8], &result.ECX, 4);
        memcpy(&m_info.modelStr[i * 16 + 12], &result.EDX, 4);
    }

    // now check if we are running on a hypervisor, and the hypervisor string
    result = x86_64_CPUID(1, 0);

    if ((result.ECX & (1 << 31)) > 0) {
        x86_64_CPUIDResult result2 = x86_64_CPUID(0x40000000, 0);
        m_info.maxHypervisorCPUID = result2.EAX;

        memcpy(&m_info.hypervisorStr[0], &result2.EBX, 4);
        memcpy(&m_info.hypervisorStr[4], &result2.ECX, 4);
        memcpy(&m_info.hypervisorStr[8], &result2.EDX, 4);

#define HYPER_STR_CMP(a) strncmp(m_info.hypervisorStr, a, 12) == 0

        if (HYPER_STR_CMP("TCGTCGTCGTCG"))
            m_info.hypervisor = x86_HYPER_QEMU;
        else if (HYPER_STR_CMP("KVMKVMKVM   "))
            m_info.hypervisor = x86_HYPER_KVM;
        else if (HYPER_STR_CMP("VMwareVMware"))
            m_info.hypervisor = x86_HYPER_VMWARE;
        else if (HYPER_STR_CMP("VboxVboxVbox"))
            m_info.hypervisor = x86_HYPER_VIRTUALBOX;
        else if (HYPER_STR_CMP("XenVMMXenVMM"))
            m_info.hypervisor = x86_HYPER_XEN;
        else if (HYPER_STR_CMP("Microsoft Hv"))
            m_info.hypervisor = x86_HYPER_HYPERV;
        else if (HYPER_STR_CMP(" prl hyperv "))
            m_info.hypervisor = x86_HYPER_PARALLELS;
        else
            m_info.hypervisor = x86_HYPER_UNKNOWN;

#undef HYPER_STR_CMP
    } else
        m_info.hypervisor = x86_HYPER_NONE;
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

const x86_64_CPUInfo* x86_64_Processor::GetCPUInfo() const {
    return &m_info;
}


void x86_64_AP_Init(x86_64_Processor* proc, uint64_t stackTop) {
    return proc->Init(stackTop);
}
