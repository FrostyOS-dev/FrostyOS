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

#include "LAPIC.hpp"
// #include "IPI.hpp"

#include "../ISR.hpp"

#include "../../MSR.h"
#include "../../Panic.hpp"
#include "../../Memory/PageTables.hpp"

#include <assert.h>
#include <string.h>
#include <util.h>
// #include <stdio.h>

#include <Memory/PagingUtil.hpp>
#include <Memory/PageTable.hpp>

#define x86_64_MSR_APIC_BASE 0x1B

x86_64_LAPIC BSP_LAPIC(true);

x86_64_LAPIC* g_BSP_LAPIC = nullptr;

void x86_64_LAPIC_SpuriousInterruptHandler(x86_64_ISR_Frame* frame) {
    x86_64_Panic("LAPIC Spurious Interrupt Occurred", frame, true);
}

x86_64_LAPIC::x86_64_LAPIC(bool BSP, uint8_t ID) : m_BSP(BSP), m_LAPICBase(0), m_LAPICID(ID), m_addressOverride(false), m_NMISources{{0xFF, false, false}, {0xFF, false, false}} {
    if (BSP)
        g_BSP_LAPIC = this;
}

x86_64_LAPIC::~x86_64_LAPIC() {

}

void x86_64_LAPIC::SetAddressOverride(void* address) {
    m_addressOverride = true;
    m_LAPICBase = (uint64_t)to_HHDM(address);
}

void x86_64_LAPIC::Init(bool started) {
    if (!m_addressOverride) {
        // Read the LAPIC base address from the MSR
        m_LAPICBase = to_HHDM(x86_64_ReadMSR(x86_64_MSR_APIC_BASE) & 0xFFFFF000);
    }

    if (!started)
        return StartCPU();

    // Read the LAPIC ID
    m_LAPICID = (ReadRegister(x86_64_LAPIC_Register::LAPIC_ID) >> 24) & 0xFF;
    

    // mask all the LVTs
    for (uint8_t i = 0; i < ((uint64_t)x86_64_LAPIC_Register::LVT_ERROR - (uint64_t)x86_64_LAPIC_Register::CMCI_LVT); i++) {
        if (i == 1 || i == 2)
            continue;
        uint32_t value = ReadRegister((uint64_t)x86_64_LAPIC_Register::CMCI_LVT + i * 0x10);
        value |= 1 << 16;
        WriteRegister((uint64_t)x86_64_LAPIC_Register::CMCI_LVT + i * 0x10, value);
    }

    // setup NMIs
    for (uint8_t i = 0; i < 2; i++) {
        if (m_NMISources[i].LINT == 0xFF)
            continue;
        uint32_t value = ReadRegister((uint64_t)x86_64_LAPIC_Register::LVT_LINT0 + i * 0x10);
        value &= 0xFFFF'0800; // keep it masked
        value |= m_NMISources[i].activeLow ? 0 : 1 << 13;
        value |= m_NMISources[i].levelTriggered ? 0 : 1 << 15;
        value |= 0b100 << 8; // NMI
        WriteRegister((uint64_t)x86_64_LAPIC_Register::LVT_LINT0 + i * 0x10, value);
    }

    x86_64_ISR_RegisterHandler(0xFF, x86_64_LAPIC_SpuriousInterruptHandler);

    uint32_t spurious_vector = ReadRegister(x86_64_LAPIC_Register::SVR) & 0xFFFF'8C00;
    spurious_vector |= 0xFF;
    spurious_vector |= 0x100;
    WriteRegister(x86_64_LAPIC_Register::SVR, spurious_vector);
}

void x86_64_LAPIC::AddNMISource(uint8_t LINT, bool activeLow, bool levelTriggered) {
    assert(LINT < 2);
    m_NMISources[LINT].LINT = LINT;
    m_NMISources[LINT].activeLow = activeLow;
    m_NMISources[LINT].levelTriggered = levelTriggered;
}

extern "C" {

    extern void x86_64_AP_trampoline();

}

void x86_64_LAPIC::StartCPU() {
    return; // for now
    // uint32_t* CR3 = (uint32_t*)0xFFC;
    x86_64_MapPage(g_Level4Table, 0, 0, 0x08000003);
    x86_64_InvalidatePage(g_Level4Table, 0);
    memcpy((void*)0, (void*)&x86_64_AP_trampoline, 0x1000);

}

void x86_64_LAPIC::WriteRegister(x86_64_LAPIC_Register reg, uint32_t value) {
    volatile_addr_write32(m_LAPICBase + (uint64_t)reg, value);
}

uint32_t x86_64_LAPIC::ReadRegister(x86_64_LAPIC_Register reg) {
    return volatile_addr_read32(m_LAPICBase + (uint64_t)reg);
}

void x86_64_LAPIC::WriteRegister(uint64_t reg, uint32_t value) {
    volatile_addr_write32(m_LAPICBase + (uint64_t)reg, value);
}

uint32_t x86_64_LAPIC::ReadRegister(uint64_t reg) {
    return volatile_addr_read32(m_LAPICBase + (uint64_t)reg);
}

uint8_t x86_64_LAPIC::GetID() {
    return m_LAPICID;
}
