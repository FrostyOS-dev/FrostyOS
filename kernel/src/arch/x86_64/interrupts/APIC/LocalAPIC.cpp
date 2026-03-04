/*
Copyright (©) 2026  Frosty515

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

#include "IPI.hpp"
#include "LocalAPIC.hpp"

#include "../IDT.hpp"
#include "../ISR.hpp"

#include "../../GDT.hpp"
#include "../../MSR.h"
#include "../../Panic.hpp"
#include "../../Processor.hpp"

#include "../../Memory/PagingInit.hpp"
#include "../../Memory/PagingUtil.hpp"

#include <spinlock.h>
#include <stdint.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <HAL/Processor.hpp>
#include <HAL/Time.hpp>

#include <HAL/drivers/HPET.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/PagingUtil.hpp>
#include <Memory/VMM.hpp>

#define x86_64_MSR_APIC_BASE 0x1B

#define HPET_WAIT_NS 10'000'000

x86_64_LAPIC BSP_LAPIC(true);

x86_64_LAPIC* g_BSP_LAPIC = nullptr;

spinlock_t g_APTrampLock = SPINLOCK_DEFAULT_VALUE;

uint8_t g_LAPICDivisorLookup[8] = {
    0b1011,
    0b0000,
    0b0001,
    0b0010,
    0b0011,
    0b1000,
    0b1001,
    0b1010,
};

void x86_64_LAPIC_SpuriousInterruptHandler(x86_64_ISR_Frame* frame) {
    x86_64_Panic("LAPIC Spurious Interrupt Occurred", frame, true);
}

void x86_64_LAPIC_TimerInterrupt(x86_64_ISR_Frame* frame) {
    x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    if (proc == nullptr)
        proc = static_cast<x86_64_Processor*>(g_BSP);

    // If proc is null at this point, something is very wrong, and we should just take the page fault

    x86_64_LAPIC* lapic = proc->GetLAPIC();
    if (lapic == nullptr && proc == g_BSP)
        lapic = g_BSP_LAPIC;

    if (lapic == nullptr)
        x86_64_Panic("LAPIC Timer interrupt occurred, but no LAPIC", frame, true);

    lapic->TimerInterrupt(proc, frame);
}

x86_64_LAPIC::x86_64_LAPIC(bool BSP, uint8_t ID) : m_BSP(BSP), m_ID(ID), m_LAPICBase(0), m_addressOverride(false), m_NMISources({false, false, false}, {false, false, false}), m_timerPeriod(0) {
    if (BSP)
        g_BSP_LAPIC = this;
}

void x86_64_LAPIC::SetAddressOverride(uint64_t address) {
    m_addressOverride = true;
    m_LAPICBase = to_HHDM(address);
}

void x86_64_LAPIC::Init(bool started) {
    if (!m_addressOverride)
        m_LAPICBase = to_HHDM(x86_64_ReadMSR(x86_64_MSR_APIC_BASE) & 0xFFFFF000);

    if (m_BSP)
        g_KPageMapper->MapPage(m_LAPICBase, from_HHDM(m_LAPICBase), VMM::Protection::READ_WRITE, VMM::CacheType::UNCACHABLE);

    if (!started)
        return StartCPU();

    m_ID = (ReadRegister(x86_64_LAPIC_Register::LAPIC_ID) >> 24) & 0xFF; // Read the ID

    // mask all the LVTs
    for (uint8_t i = 0; i < (((uint64_t)x86_64_LAPIC_Register::LVT_ERR - (uint64_t)x86_64_LAPIC_Register::LVT_CMCI) / 0x10); i++) {
        if (i == 1 || i == 2)
            continue;
        uint32_t value = ReadRegister((uint64_t)x86_64_LAPIC_Register::LVT_CMCI + i * 0x10);
        value |= 1 << 16;
        WriteRegister((uint64_t)x86_64_LAPIC_Register::LVT_CMCI + i * 0x10, value);
    }

    // setup NMIs
    for (uint8_t i = 0; i < 2; i++) {
        if (!m_NMISources[i].used)
            continue;
        uint32_t value = ReadRegister((uint64_t)x86_64_LAPIC_Register::LVT_LINT0 + i * 0x10);
        value &= 0xFFFF'0800; // keep it masked
        value |= m_NMISources[i].activeLow ? 0 : 1 << 13;
        value |= m_NMISources[i].levelTriggered ? 0 : 1 << 15;
        value |= 0b100 << 8; // NMI
        WriteRegister((uint64_t)x86_64_LAPIC_Register::LVT_LINT0 + i * 0x10, value);
    }

    if (m_BSP)
        x86_64_ISR_RegisterHandler(0xFF, x86_64_LAPIC_SpuriousInterruptHandler);

    uint32_t spuriousVector = ReadRegister(x86_64_LAPIC_Register::SpuriousIV) & 0xFFFF8C00;
    spuriousVector |= 0xFF;
    spuriousVector |= 0x100; // enable
    WriteRegister(x86_64_LAPIC_Register::SpuriousIV, spuriousVector);

    if (m_BSP)
        static_cast<x86_64_Processor*>(g_BSP)->SetLAPIC(this);
}

bool x86_64_LAPIC::InitTimer() {
    if (m_timerPeriod == 0) {
        if (g_HPET == nullptr)
            return false;

        g_HPET->Lock();
        uint64_t* HPETCounter = g_HPET->GetMainCounterPointer();
        uint32_t HPETPeriod = g_HPET->GetPeriod();

        // Start by setting divide config
        uint32_t value = ReadRegister(x86_64_LAPIC_Register::DivideConfig);
        value |= 0xB; // divide by 1
        WriteRegister(x86_64_LAPIC_Register::DivideConfig, value);

        // Next set the LVT entry
        value = ReadRegister(x86_64_LAPIC_Register::LVT_Timer);
        value &= ~0x60000; // set timer mode to one-shot
        value |= 0x10000; // ensure it is masked
        WriteRegister(x86_64_LAPIC_Register::LVT_Timer, value);

        volatile uint32_t* initial = (volatile uint32_t*)(reinterpret_cast<uint64_t>(m_LAPICBase) + static_cast<int>(x86_64_LAPIC_Register::InitialCount));
        volatile uint32_t* current = (volatile uint32_t*)(reinterpret_cast<uint64_t>(m_LAPICBase) + static_cast<int>(x86_64_LAPIC_Register::CurrentCount));
        uint64_t HPETStart = volatile_addr_read64(HPETCounter);
        uint64_t HPETEnd = HPETStart + HPET_WAIT_NS * 1'000'000UL / HPETPeriod;
        *initial = UINT32_MAX;
        while (volatile_addr_read64(HPETCounter) < HPETEnd)
            __asm__ volatile ("pause" ::: "memory");
        uint32_t end = *current;
        *initial = 0; // stop counting down

        g_HPET->Unlock();

        if (end == 0 || end == UINT32_MAX) // was either too fast or too slow
            return false;

        uint64_t period = HPET_WAIT_NS * 1'000UL / (UINT32_MAX - end);
        uint64_t freq = ALIGN_UP(1'000'000'000'000 / period, 100'000);
        m_timerPeriod = 1'000'000'000'000 / freq; // picoseconds
    }

    if (m_timerPeriod > LAPIC_TIMER_PERIOD)
        return false; // timer is too slow

    int divisor;

    for (divisor = 7; divisor > 0; divisor--) {
        if ((1 << divisor) * m_timerPeriod <= LAPIC_TIMER_PERIOD)
            break;
    }

    if (divisor < 0)
        return false; // shouldn't happen, just to be safe

    m_timerDivisor = 1 << divisor;
    m_timerTicks = LAPIC_TIMER_PERIOD / (m_timerPeriod * m_timerDivisor);

    // Register the ISR Handler
    if (m_BSP)
        x86_64_ISR_RegisterHandler(LAPIC_TIMER_INT, x86_64_LAPIC_TimerInterrupt);

    // Start by setting divide config
    uint32_t value = ReadRegister(x86_64_LAPIC_Register::DivideConfig);
    value &= 0xB;
    value |= g_LAPICDivisorLookup[m_timerDivisor];
    WriteRegister(x86_64_LAPIC_Register::DivideConfig, value);

    // Next set the LVT entry
    value = ReadRegister(x86_64_LAPIC_Register::LVT_Timer);
    value &= ~0x700FF; // clear mode, vector, and mask
    value |= 0x20000 | LAPIC_TIMER_INT; // unmasked, and periodic mode. Safe to unmask at this stage as interrupts still need to be enabled for it to do anything.
    WriteRegister(x86_64_LAPIC_Register::LVT_Timer, value);

    WriteRegister(x86_64_LAPIC_Register::InitialCount, m_timerTicks);
    
    return true;
}

void x86_64_LAPIC::AddNMISource(uint8_t LINT, bool activeLow, bool levelTriggered) {
    if (LINT > 1)
        return;
    m_NMISources[LINT] = {true, activeLow, levelTriggered};
}

void x86_64_LAPIC::StartCPU() {
    if ((uint64_t)from_HHDM(g_KernelRootPageTable) > UINT32_MAX)
        PANIC("Unable to start AP, root page table is too high in physical memory");

    spinlock_acquire(&g_APTrampLock);

    void* stack = VMM::g_KVMM->AllocatePages(KERNEL_STACK_SIZE >> PAGE_SIZE_SHIFT, VMM::Protection::READ_WRITE, true);
    if (stack == nullptr)
        PANIC("Failed to allocate stack for AP");
    memset(stack, 0, KERNEL_STACK_SIZE);

    x86_64_Processor* proc = new x86_64_Processor(false);
    proc->SetLAPIC(this);

    g_KPageMapper->MapPage(AP_TRAMP_LOAD, AP_TRAMP_LOAD, VMM::Protection::READ_WRITE_EXECUTE, VMM::CacheType::DEFAULT);

    memcpy(AP_TRAMP_LOAD_ADDR, (void*)&x86_64_APTrampoline, PAGE_SIZE);

    x86_64_APInfo info;
    info.cr3 = (uint32_t)from_HHDM((uint64_t)g_KernelRootPageTable);
    info.cr4Extras = x86_64_Is5LevelPagingSupported() ? 1 << 12 : 0;
    info.stackEnd = (uint64_t)stack + KERNEL_STACK_SIZE;
    info.func = (uint64_t)&x86_64_AP_Init;
    info.proc = proc;
    info.GDTR = x86_64_CreateGDTR();
    info.KCS = x86_64_GDT_KERNEL_CODE_SEGMENT;
    info.KDS = x86_64_GDT_KERNEL_DATA_SEGMENT;
    info.IDTR = x86_64_CreateIDTR();

    memcpy(AP_TRAMP_DATA_ADDR, &info, sizeof(x86_64_APInfo));

    dbgprintf("Starting AP %hhu\n", m_ID);

    {
        using namespace x86_64_IPI;

        RaiseIPI(this, m_ID, 0, DeliveryMode::INIT);
        HAL_SleepNS(10 * 1'000'000); // 10ms

        // Intel suggests sending two SIPIs
        RaiseIPI(this, m_ID, 0, DeliveryMode::Startup);
        HAL_SleepNS(200 * 1000); // 200us
        RaiseIPI(this, m_ID, 0, DeliveryMode::Startup);
        HAL_SleepNS(200 * 1000); // 200us

    }
    spinlock_acquire(&proc->apLock);

    dbgprintf("AP %hhu is online!\n", m_ID);

    g_KPageMapper->UnmapPage(AP_TRAMP_LOAD);

    spinlock_release(&g_APTrampLock);
}

void x86_64_LAPIC::WriteRegister(x86_64_LAPIC_Register reg, uint32_t value) {
    volatile_addr_write32(reinterpret_cast<uint64_t>(m_LAPICBase) + static_cast<int>(reg), value);
}

uint32_t x86_64_LAPIC::ReadRegister(x86_64_LAPIC_Register reg) {
    return volatile_addr_read32(reinterpret_cast<uint64_t>(m_LAPICBase) + static_cast<int>(reg));
}

void x86_64_LAPIC::WriteRegister(uint64_t reg, uint32_t value) {
    volatile_addr_write32(m_LAPICBase + reg, value);
}

uint32_t x86_64_LAPIC::ReadRegister(uint64_t reg) {
    return volatile_addr_read32(m_LAPICBase + reg);
}

void x86_64_LAPIC::SendEOI() {
    WriteRegister(x86_64_LAPIC_Register::EOI, 0);
}

uint8_t x86_64_LAPIC::GetID() const {
    return m_ID;
}

void x86_64_LAPIC::SetTimerPeriod(uint64_t period) {
    m_timerPeriod = period;
}

void x86_64_LAPIC::TimerInterrupt(x86_64_Processor* proc, x86_64_ISR_Frame* frame) {
    HAL_TimerTick(proc, LAPIC_TIMER_PERIOD / 1'000'000'000, frame);
    SendEOI();
}
