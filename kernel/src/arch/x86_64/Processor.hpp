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

#ifndef _x86_64_PROCESSOR_HPP
#define _x86_64_PROCESSOR_HPP

#include <stdint.h>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include <Memory/MemoryMap.hpp>

#include <Scheduling/Scheduler.hpp>

#include "interrupts/IRQ.hpp"

#include "interrupts/APIC/LocalAPIC.hpp"

enum x86_Vendor {
    x86_VENDOR_INTEL,
    x86_VENDOR_CYRIX,
    x86_VENDOR_AMD,
    x86_VENDOR_UMC,
    x86_VENDOR_CENTAUR,
    x86_VENDOR_TRANSMETA,
    x86_VENDOR_NSC,
    x86_VENDOR_HYGON,
    x86_VENDOR_ZHAOXIN,
    x86_VENDOR_VORTEX,
    x86_VENDOR_UNKNOWN = 0xFF
};

enum x86_Hypervisor {
    x86_HYPER_NONE,
    x86_HYPER_QEMU,
    x86_HYPER_KVM,
    x86_HYPER_VMWARE,
    x86_HYPER_VIRTUALBOX,
    x86_HYPER_XEN,
    x86_HYPER_HYPERV,
    x86_HYPER_PARALLELS,
    x86_HYPER_UNKNOWN = 0xFF
};
 
struct x86_64_CPUInfo {
    uint8_t vendor;
    char vendorStr[12];
    char modelStr[48];
    uint32_t maxCPUID;
    uint32_t maxExtendedCPUID;
    uint8_t hypervisor;
    char hypervisorStr[12];
    uint32_t maxHypervisorCPUID;
};

class x86_64_Processor final : public Processor {
public:
    x86_64_Processor(bool BSP);
    ~x86_64_Processor();

    void Init() override;
    void Init(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) override;

    void FillCPUInfo();

    void InitTime() override;

    void SetIRQData(x86_64_ProcessorIRQData* data);
    x86_64_ProcessorIRQData* GetIRQData();

    void SetLAPIC(x86_64_LAPIC* lapic);
    x86_64_LAPIC* GetLAPIC() const;

    // Will only return null if this is null
    const x86_64_CPUInfo* GetCPUInfo() const;

private:
    x86_64_CPUInfo m_info;
    x86_64_ProcessorIRQData* m_IRQData;
    x86_64_LAPIC* m_LAPIC;
    bool m_TSCAvailable;
};

extern x86_64_Processor g_x86_64_BSP;

extern "C" Processor* GetCurrentProcessor();
extern "C" Scheduler::ProcessorState* GetCurrentProcessorState();

#endif /* _x86_64_PROCESSOR_HPP */