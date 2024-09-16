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

#include "../ISR.hpp"

#include "../../MSR.h"
#include "../../Panic.hpp"

#include <util.h>

#include <Memory/PagingUtil.hpp>

#define x86_64_MSR_APIC_BASE 0x1B

x86_64_LAPIC BSP_LAPIC(true);

x86_64_LAPIC* g_BSP_LAPIC = nullptr;

void x86_64_LAPIC_SpuriousInterruptHandler(x86_64_ISR_Frame* frame) {
    x86_64_Panic("LAPIC Spurious Interrupt Occurred", frame, true);
}

x86_64_LAPIC::x86_64_LAPIC(bool BSP) : m_BSP(BSP), m_LAPICBase(0), m_LAPICID(0) {
    if (BSP)
        g_BSP_LAPIC = this;
}

x86_64_LAPIC::~x86_64_LAPIC() {

}

void x86_64_LAPIC::Init() {
    // Read the LAPIC base address from the MSR
    m_LAPICBase = to_HHDM(x86_64_ReadMSR(x86_64_MSR_APIC_BASE) & 0xFFFFF000);
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


    x86_64_ISR_RegisterHandler(0xFF, x86_64_LAPIC_SpuriousInterruptHandler);

    uint8_t spurious_vector = ReadRegister(x86_64_LAPIC_Register::SVR) & 0xFFFF'8C00;
    spurious_vector |= 0xFF;
    spurious_vector |= 0x100;
    WriteRegister(x86_64_LAPIC_Register::SVR, spurious_vector);
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
