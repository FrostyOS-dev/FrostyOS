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

#include "IOAPIC.hpp"

#include <stdint.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/PagingUtil.hpp>

#include <Memory/PagingUtil.hpp>

struct x86_64_IRQRedirectionEntry {
    uint8_t source;
    uint32_t GSI;
};

LinkedList::LockableLinkedList<x86_64_IOAPIC> g_IOAPICs;
LinkedList::LockableLinkedList<x86_64_IRQRedirectionEntry> g_IRQRedirections;

x86_64_IOAPIC::x86_64_IOAPIC(uint64_t address, uint32_t GSIBase) : m_ID(0xFF), m_baseAddress(address), m_GSIBase(GSIBase) {

}

void x86_64_IOAPIC::Init() {
    g_KPageMapper->MapPage(m_baseAddress, from_HHDM(m_baseAddress), VMM::Protection::READ_WRITE, VMM::CacheType::UNCACHABLE);

    m_ID = (ReadRegister(x86_64_IOAPIC_Register::ID) >> 24) & 0xFF;
    m_maxRedirectionEntries = ((ReadRegister(x86_64_IOAPIC_Register::VER) >> 16) & 0xFF) + 1;

    for (uint8_t i = 0; i < m_maxRedirectionEntries; i++) {
        x86_64_IOAPIC_RedirectionEntry entry = GetRedirectionEntry(i);
        entry.Vector = 0;
        entry.DeliveryMode = 0; // Fixed
        entry.DestMode = 0; // Physical
        entry.IntPolarity = 0; // active high
        entry.TriggerMode = 0; // edge sensitive
        entry.Masked = 1;
        SetRedirectionEntry(i, entry);
    }

    g_IOAPICs.lock();
    g_IOAPICs.insert(this);
    g_IOAPICs.unlock();
}

void x86_64_IOAPIC::WriteRegister(x86_64_IOAPIC_Register reg, uint32_t value) {
    uint32_t buffer = volatile_addr_read32(m_baseAddress);
    buffer &= ~0xFF;
    buffer |= static_cast<uint8_t>(reg);
    volatile_addr_write32(m_baseAddress, buffer);
    volatile_addr_write32(m_baseAddress + 16, value);
}

uint32_t x86_64_IOAPIC::ReadRegister(x86_64_IOAPIC_Register reg) {
    uint32_t buffer = volatile_addr_read32(m_baseAddress);
    buffer &= ~0xFF;
    buffer |= static_cast<uint8_t>(reg);
    volatile_addr_write32(m_baseAddress, buffer);
    return volatile_addr_read32(m_baseAddress + 16);
}

void x86_64_IOAPIC::WriteRegister(uint8_t reg, uint32_t value) {
    uint32_t buffer = volatile_addr_read32(m_baseAddress);
    buffer &= ~0xFF;
    buffer |= reg;
    volatile_addr_write32(m_baseAddress, buffer);
    volatile_addr_write32(m_baseAddress + 16, value);
}

uint32_t x86_64_IOAPIC::ReadRegister(uint8_t reg) {
    uint32_t buffer = volatile_addr_read32(m_baseAddress);
    buffer &= ~0xFF;
    buffer |= reg;
    volatile_addr_write32(m_baseAddress, buffer);
    return volatile_addr_read32(m_baseAddress + 16);
}

void x86_64_IOAPIC::SetRedirectionEntry(uint8_t index, x86_64_IOAPIC_RedirectionEntry entry) {
    if (index > m_maxRedirectionEntries)
        return;
    uint32_t* buffer = reinterpret_cast<uint32_t*>(&entry);
    WriteRegister(static_cast<uint8_t>(x86_64_IOAPIC_Register::REDTBL_0) + index * 2, buffer[0]);
    WriteRegister(static_cast<uint8_t>(x86_64_IOAPIC_Register::REDTBL_0) + index * 2 + 1, buffer[1]);
}

x86_64_IOAPIC_RedirectionEntry x86_64_IOAPIC::GetRedirectionEntry(uint8_t index) {
    if (index > m_maxRedirectionEntries)
        return {};
    uint32_t buffer[2] = {0, 0};
    buffer[0] = ReadRegister(static_cast<uint8_t>(x86_64_IOAPIC_Register::REDTBL_0) + index * 2);
    buffer[1] = ReadRegister(static_cast<uint8_t>(x86_64_IOAPIC_Register::REDTBL_0) + index * 2 + 1);
    x86_64_IOAPIC_RedirectionEntry* entry = reinterpret_cast<x86_64_IOAPIC_RedirectionEntry*>(buffer);
    return *entry;
}

uint8_t x86_64_IOAPIC::GetID() {
    return m_ID;
}

uint32_t x86_64_IOAPIC::GetGSIBase() {
    return m_GSIBase;
}

uint32_t x86_64_IOAPIC::GetMaxGSI() {
    return m_GSIBase + m_maxRedirectionEntries - 1;
}


x86_64_IOAPIC* x86_64_GetIOAPICForGSI(uint32_t GSI) {
    struct Data {
        uint32_t GSI;
        x86_64_IOAPIC* out;
    } d = {GSI, nullptr};
    g_IOAPICs.lock();
    g_IOAPICs.Enumerate([](x86_64_IOAPIC* ioapic, void* data) -> bool {
        Data* d = static_cast<Data*>(data);
        if (ioapic->GetGSIBase() <= d->GSI && ioapic->GetMaxGSI() >= d->GSI) {
            d->out = ioapic;
            return false;
        }
        return true;
    }, &d);
    g_IOAPICs.unlock();
    return d.out; // will stay nullptr if not found
}

void x86_64_IOAPIC_SetRedirection(uint8_t source, uint32_t GSI, bool activeLow, bool level) {
    x86_64_IOAPIC* ioapic = x86_64_GetIOAPICForGSI(GSI);
    if (ioapic == nullptr)
        return;

    
    uint8_t index = GSI - ioapic->GetGSIBase();
    x86_64_IOAPIC_RedirectionEntry entry = ioapic->GetRedirectionEntry(index);
    entry.IntPolarity = activeLow ? 1 : 0;
    entry.TriggerMode = level ? 1 : 0;
    ioapic->SetRedirectionEntry(index, entry);

    x86_64_SetGSIRedirection(source, GSI);
}

void x86_64_SetGSIRedirection(uint8_t source, uint32_t GSI) {
    if (source != GSI) {
        x86_64_IRQRedirectionEntry* entry = new x86_64_IRQRedirectionEntry;
        entry->source = source;
        entry->GSI = GSI;
        g_IRQRedirections.lock();
        g_IRQRedirections.insert(entry);
        g_IRQRedirections.unlock();
    }
}

uint32_t x86_64_GetGSIFromSource(uint8_t source) {
    g_IRQRedirections.lock();
    uint32_t out = source;
    g_IRQRedirections.Enumerate([](x86_64_IRQRedirectionEntry* entry, void* data) -> bool {
        uint32_t* out = static_cast<uint32_t*>(data);
        if (entry->source == *out) {
            *out = entry->GSI;
            return false;
        }
        return true;
    }, &out);
    g_IRQRedirections.unlock();
    return out;
}
