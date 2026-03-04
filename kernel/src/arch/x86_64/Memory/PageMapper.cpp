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

#include "PageMapper.hpp"
#include "PageTables.hpp"
#include "PagingUtil.hpp"
#include "PAT.hpp"

#include "../interrupts/NMI.hpp"
#include "arch/x86_64/Processor.hpp"
#include "arch/x86_64/interrupts/APIC/LocalAPIC.hpp"

#include <util.h>

#include <Memory/VMM.hpp>

x86_64_PageMapper::x86_64_PageMapper() : m_pageTable(nullptr) {
    
}

x86_64_PageMapper::x86_64_PageMapper(void* pageTable) : m_pageTable(pageTable) {

}

x86_64_PageMapper::~x86_64_PageMapper() {

}

bool x86_64_PageMapper::MapPage(uint64_t virt, uint64_t phys, VMM::Protection prot, VMM::CacheType cacheType) {
    uint32_t flags = 1; // Present
    switch (prot) {
    case VMM::Protection::READ:
        flags |= 0x800'0000; // Read-only, No execute
        break;
    case VMM::Protection::READ_WRITE:
        flags |= 0x800'0002; // Read-write, No execute
        break;
    case VMM::Protection::READ_EXECUTE:
        flags |= 0; // Execute
        break;
    case VMM::Protection::READ_WRITE_EXECUTE:
        flags |= 2; // Read-write + execute
        break;
    default:
        return false; // Invalid protection
    }
    x86_64_PATOffset offset = x86_64_PATOffset::Default;
    switch (cacheType) {
    case VMM::CacheType::UNCACHABLE:
        offset = x86_64_PATOffset::Uncachable;
        break;
    case VMM::CacheType::WRITE_BACK:
        offset = x86_64_PATOffset::WriteBack;
        break;
    case VMM::CacheType::WRITE_THROUGH:
        offset = x86_64_PATOffset::WriteThrough;
        break;
    case VMM::CacheType::WRITE_PROTECTED:
        offset = x86_64_PATOffset::WriteProtected;
        break;
    case VMM::CacheType::WRITE_COMBINING:
        offset = x86_64_PATOffset::WriteCombining;
        break;
    }
    flags |= x86_64_PAT_GetPageMappingFlags(offset);
    x86_64_MapPage(m_pageTable, virt, phys, flags);
    return true;
}

bool x86_64_PageMapper::MapPages(uint64_t virt, uint64_t phys, size_t count, VMM::Protection prot, VMM::CacheType cacheType) {
    for (size_t i = 0; i < count; i++) {
        if (!MapPage(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, prot, cacheType))
            return false;
    }
    return true;
}

bool x86_64_PageMapper::UnmapPage(uint64_t virt) {
    x86_64_UnmapPage(m_pageTable, virt);
    return true;
}

bool x86_64_PageMapper::UnmapPages(uint64_t virt, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!UnmapPage(virt + i * PAGE_SIZE))
            return false;
    }
    return true;
}

bool x86_64_PageMapper::RemapPage(uint64_t virt, VMM::Protection prot, VMM::CacheType cacheType) {
    uint32_t flags = 1; // Present
    switch (prot) {
    case VMM::Protection::READ:
        flags |= 0x800'0000; // Read-only, No execute
        break;
    case VMM::Protection::READ_WRITE:
        flags |= 0x800'0002; // Read-write, No execute
        break;
    case VMM::Protection::READ_EXECUTE:
        flags |= 0; // Execute
        break;
    case VMM::Protection::READ_WRITE_EXECUTE:
        flags |= 2; // Read-write + execute
        break;
    default:
        return false; // Invalid protection
    }
    x86_64_PATOffset offset = x86_64_PATOffset::Default;
    switch (cacheType) {
    case VMM::CacheType::UNCACHABLE:
        offset = x86_64_PATOffset::Uncachable;
        break;
    case VMM::CacheType::WRITE_BACK:
        offset = x86_64_PATOffset::WriteBack;
        break;
    case VMM::CacheType::WRITE_THROUGH:
        offset = x86_64_PATOffset::WriteThrough;
        break;
    case VMM::CacheType::WRITE_PROTECTED:
        offset = x86_64_PATOffset::WriteProtected;
        break;
    case VMM::CacheType::WRITE_COMBINING:
        offset = x86_64_PATOffset::WriteCombining;
        break;
    }
    flags |= x86_64_PAT_GetPageMappingFlags(offset);
    x86_64_RemapPage(m_pageTable, virt, flags);
    return true;
}

bool x86_64_PageMapper::RemapPages(uint64_t virt, size_t count, VMM::Protection prot, VMM::CacheType cacheType) {
    for (size_t i = 0; i < count; i++) {
        if (!RemapPage(virt + i * PAGE_SIZE, prot, cacheType))
            return false;
    }
    return true;
}

void x86_64_PageMapper::InvalidatePages(uint64_t virt, size_t count, bool shootdown) {
    x86_64_InvalidatePages(virt, count * PAGE_SIZE);
    if (shootdown) {
        x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
        if (proc != nullptr) {
            x86_64_NMI_InvPagesData data = {virt, count};
            x86_64_LAPIC* lapic = proc->GetLAPIC();
            if (lapic != nullptr)
                x86_64_GlobalNMI::Raise(lapic, x86_64_NMIType::INVPAGES, &data);
        }
    }
}

void x86_64_PageMapper::SetPageTable(void* pageTable) {
    m_pageTable = pageTable;
}

void* x86_64_PageMapper::GetPageTable() const {
    return m_pageTable;
}

bool x86_64_PageMapper::isPermsReduction(VMM::Protection oldProt, VMM::Protection newProt) const {
    using namespace VMM;
    switch (newProt) {
    case Protection::READ:
        return oldProt != newProt;
    case Protection::WRITE:
    case Protection::READ_WRITE:
        return oldProt != Protection::EXECUTE && oldProt != Protection::READ_EXECUTE && oldProt != Protection::READ_WRITE_EXECUTE;
    case Protection::EXECUTE:
    case Protection::READ_EXECUTE:
        return oldProt != Protection::WRITE && oldProt != Protection::READ_WRITE && oldProt != Protection::READ_WRITE_EXECUTE;
    case Protection::READ_WRITE_EXECUTE:
        return false;
    }
    return true;
}
