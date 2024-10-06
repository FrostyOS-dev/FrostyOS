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

#include "PageTable.hpp"
#include "PageTables.hpp"
#include "PAT.hpp"

#include <util.h>

x86_64_PageTable::x86_64_PageTable(x86_64_Level4Table* table, bool user) : PageTable(user), m_table(table) {

}

x86_64_PageTable::~x86_64_PageTable() {

}

void x86_64_PageTable::Map(void* virtualAddress, void* physicalAddress, PagePermission permission, PageCache cache, bool flush, PageType type) {
    uint32_t flags = DecodePageFlags(permission, cache, type);
    switch (type) {
    case PageType::NORMAL:
        x86_64_MapPage(m_table, (uint64_t)virtualAddress, (uint64_t)physicalAddress, flags);
        break;
    case PageType::LARGE:
        x86_64_Map2MiBPage(m_table, (uint64_t)virtualAddress, (uint64_t)physicalAddress, flags);
        break;
    case PageType::HUGE:
        x86_64_Map1GiBPage(m_table, (uint64_t)virtualAddress, (uint64_t)physicalAddress, flags);
        break;
    }
    if (flush)
        Flush(virtualAddress, type == PageType::NORMAL ? 1 : (type == PageType::LARGE ? 512 : (512 * 512)));
}

void x86_64_PageTable::Remap(void* virtualAddress, PagePermission permission, PageCache cache, bool flush, PageType type) {
    uint32_t flags = DecodePageFlags(permission, cache, type);
    switch (type) {
    case PageType::NORMAL:
        x86_64_RemapPage(m_table, (uint64_t)virtualAddress, flags);
        break;
    case PageType::LARGE:
        x86_64_Remap2MiBPage(m_table, (uint64_t)virtualAddress, flags);
        break;
    case PageType::HUGE:
        x86_64_Remap1GiBPage(m_table, (uint64_t)virtualAddress, flags);
        break;
    }
    if (flush)
        Flush(virtualAddress, type == PageType::NORMAL ? 1 : (type == PageType::LARGE ? 512 : (512 * 512)));
}

void x86_64_PageTable::Unmap(void* virtualAddress, bool flush, PageType type) {
    switch (type) {
    case PageType::NORMAL:
        x86_64_UnmapPage(m_table, (uint64_t)virtualAddress);
        break;
    case PageType::LARGE:
        x86_64_Unmap2MiBPage(m_table, (uint64_t)virtualAddress);
        break;
    case PageType::HUGE:
        x86_64_Unmap1GiBPage(m_table, (uint64_t)virtualAddress);
        break;
    }
    if (flush)
        Flush(virtualAddress, type == PageType::NORMAL ? 1 : (type == PageType::LARGE ? 512 : (512 * 512)));
}

void* x86_64_PageTable::GetPhysicalAddress(void* virtualAddress) const {
    return (void*)x86_64_GetPhysicalAddress(m_table, (uint64_t)virtualAddress);
}

void x86_64_PageTable::Flush(void* virtualAddress, uint64_t pageCount) const {
    if (pageCount > FULL_FLUSH_THRESHOLD)
        x86_64_FullTLBFlush();
    else {
        for (uint64_t i = 0; i < pageCount; i++)
            x86_64_InvalidatePage(m_table, (uint64_t)virtualAddress + i * PAGE_SIZE);
    }
}

void* x86_64_PageTable::GetRoot() const {
    return m_table;
}

void x86_64_PageTable::SetTable(x86_64_Level4Table* table) {
    m_table = table;
}

uint32_t x86_64_PageTable::DecodePageFlags(PagePermission permission, PageCache cache, PageType type) const {
    uint32_t flags = 1; // present
    switch (permission) {
    case PagePermission::READ:
        flags |= 0x0800'0000; // no execute
        break;
    case PagePermission::READ_WRITE:
    case PagePermission::WRITE:
        flags |= 1 << 1; // write
        flags |= 0x0800'0000; // no execute
        break;
    case PagePermission::READ_EXECUTE:
        break;
    }

    x86_64_PATType patType = x86_64_PATType::Default;
    switch (cache) {
    case PageCache::WRITE_BACK:
        patType = x86_64_PATType::WriteBack;
        break;
    case PageCache::WRITE_THROUGH:
        patType = x86_64_PATType::WriteThrough;
        break;
    case PageCache::WRITE_COMBINING:
        patType = x86_64_PATType::WriteCombining;
        break;
    case PageCache::WRITE_PROTECTED:
        patType = x86_64_PATType::WriteProtected;
        break;
    case PageCache::UNCACHED:
        patType = x86_64_PATType::Uncached;
        break;
    case PageCache::UNCACHED_WEAK:
        patType = x86_64_PATType::Uncacheable;
        break;
    }

    if (type == PageType::NORMAL)
        flags |= x86_64_GetPageFlagsFromPATIndex(x86_64_GetPATIndex(patType));
    else
        flags |= x86_64_GetLargePageFlagsFromPATIndex(x86_64_GetPATIndex(patType));

    if (IsUser())
        flags |= 1 << 2; // user

    return flags;
}