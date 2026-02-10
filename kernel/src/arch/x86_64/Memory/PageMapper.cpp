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

#include <util.h>

x86_64_PageMapper::x86_64_PageMapper() : m_pageTable(nullptr) {
    
}

x86_64_PageMapper::x86_64_PageMapper(void* pageTable) : m_pageTable(pageTable) {

}

x86_64_PageMapper::~x86_64_PageMapper() {

}

bool x86_64_PageMapper::MapPage(uint64_t virt, uint64_t phys, VMM::Protection prot) {
    uint32_t flags = 1; // Present
    switch (prot) {
        case VMM::Protection::READ:
            flags |= 0; // Read-only
            break;
        case VMM::Protection::READ_WRITE:
            flags |= 2; // Read-write
            break;
        case VMM::Protection::READ_EXECUTE:
            flags |= 0x800'0000; // Execute
            break;
        case VMM::Protection::READ_WRITE_EXECUTE:
            flags |= 0x800'0002; // Read-write + execute
            break;
        default:
            return false; // Invalid protection
    }
    x86_64_MapPage(m_pageTable, virt, phys, flags);
    return true;
}

bool x86_64_PageMapper::MapPages(uint64_t virt, uint64_t phys, size_t count, VMM::Protection prot) {
    for (size_t i = 0; i < count; i++) {
        if (!MapPage(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, prot))
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

bool x86_64_PageMapper::RemapPage(uint64_t virt, VMM::Protection prot) {
    uint32_t flags = 1; // Present
    switch (prot) {
        case VMM::Protection::READ:
            flags |= 0; // Read-only
            break;
        case VMM::Protection::READ_WRITE:
            flags |= 2; // Read-write
            break;
        case VMM::Protection::READ_EXECUTE:
            flags |= 0x800'0000; // Execute
            break;
        case VMM::Protection::READ_WRITE_EXECUTE:
            flags |= 0x800'0002; // Read-write + execute
            break;
        default:
            return false; // Invalid protection
    }
    x86_64_RemapPage(m_pageTable, virt, flags);
    return true;
}

bool x86_64_PageMapper::RemapPages(uint64_t virt, size_t count, VMM::Protection prot) {
    for (size_t i = 0; i < count; i++) {
        if (!RemapPage(virt + i * PAGE_SIZE, prot))
            return false;
    }
    return true;
}

void x86_64_PageMapper::InvalidatePages(uint64_t virt, size_t count) {
    x86_64_InvalidatePages(virt, count * PAGE_SIZE);
}

void x86_64_PageMapper::SetPageTable(void* pageTable) {
    m_pageTable = pageTable;
}

void* x86_64_PageMapper::GetPageTable() const {
    return m_pageTable;
}
