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

#include "LocalAPIC.hpp"

#include "../ISR.hpp"

#include "../../MSR.h"
#include "../../Panic.hpp"
#include "../../Processor.hpp"

#include "../../Memory/PageTables.hpp"
#include "../../Memory/PagingInit.hpp"
#include "../../Memory/PAT.hpp"

#include <stdint.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <HAL/Processor.hpp>

#include <Memory/PagingUtil.hpp>

#define x86_64_MSR_APIC_BASE 0x1B

x86_64_LAPIC BSP_LAPIC(true);

x86_64_LAPIC* g_BSP_LAPIC = nullptr;

void x86_64_LAPIC_SpuriousInterruptHandler(x86_64_ISR_Frame* frame) {
    x86_64_Panic("LAPIC Spurious Interrupt Occurred", frame, true);
}

x86_64_LAPIC::x86_64_LAPIC(bool BSP, uint8_t ID) : m_BSP(BSP), m_ID(ID), m_LAPICBase(0), m_addressOverride(false), m_NMISources({false, false, false}, {false, false, false}) {
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
        x86_64_MapPage(g_KernelRootPageTable, m_LAPICBase, from_HHDM(m_LAPICBase), 0x0800'0003 | x86_64_PAT_GetPageMappingFlags(x86_64_PATOffset::Uncachable));

    if (!started)
        return StartCPU();

    m_ID = (ReadRegister(x86_64_LAPIC_Register::LAPIC_ID) >> 24) & 0xFF; // Read the ID

    // mask all the LVTs
    for (uint8_t i = 0; i < ((uint64_t)x86_64_LAPIC_Register::LVT_ERR - (uint64_t)x86_64_LAPIC_Register::LVT_CMCI); i++) {
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

    x86_64_ISR_RegisterHandler(0xFF, x86_64_LAPIC_SpuriousInterruptHandler);

    uint32_t spuriousVector = ReadRegister(x86_64_LAPIC_Register::SpuriousIV) & 0xFFFF8C00;
    spuriousVector |= 0xFF;
    spuriousVector |= 0x100; // enable
    WriteRegister(x86_64_LAPIC_Register::SpuriousIV, spuriousVector);

    if (m_BSP)
        static_cast<x86_64_Processor*>(g_BSP)->SetLAPIC(this);
}

void x86_64_LAPIC::AddNMISource(uint8_t LINT, bool activeLow, bool levelTriggered) {
    if (LINT > 1)
        return;
    m_NMISources[LINT] = {true, activeLow, levelTriggered};
}

void x86_64_LAPIC::StartCPU() {
    return; // no SMP yet, don't do anything
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
