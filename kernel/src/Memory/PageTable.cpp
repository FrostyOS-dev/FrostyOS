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

PageTable::PageTable(bool user) : m_user(user) {
}

PageTable::~PageTable() {
}

void PageTable::Map(void* virtualAddress, void* physicalAddress, PagePermission permission, PageCache cache, bool flush, PageType type) {
}

void PageTable::Remap(void* virtualAddress, PagePermission permission, PageCache cache, bool flush, PageType type) {
}

void PageTable::Unmap(void* virtualAddress, bool flush, PageType type) {
}

void* PageTable::GetPhysicalAddress(void* virtualAddress) const {
    return nullptr;
}

void PageTable::Flush(void* virtualAddress, uint64_t pageCount) const {
}

void* PageTable::GetRoot() const {
    return nullptr;
}

bool PageTable::IsUser() const {
    return m_user;
}

PageTable* g_KPageTable = nullptr;