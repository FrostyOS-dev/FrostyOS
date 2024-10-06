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

#ifndef _x86_64_PAGE_TABLE_HPP
#define _x86_64_PAGE_TABLE_HPP

#include <stdint.h>

#include <Memory/PageTable.hpp>

#include "PageTables.hpp"

// more than equivalent to 1GiB page
#define FULL_FLUSH_THRESHOLD (512 * 512)

class x86_64_PageTable : public PageTable {
public:
    x86_64_PageTable(x86_64_Level4Table* table, bool user = false);
    virtual ~x86_64_PageTable();

    virtual void Map(void* virtualAddress, void* physicalAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT, bool flush = true, PageType type = PageType::NORMAL) override;
    virtual void Remap(void* virtualAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT, bool flush = true, PageType type = PageType::NORMAL) override;
    virtual void Unmap(void* virtualAddress, bool flush = true, PageType type = PageType::NORMAL) override;
    virtual void* GetPhysicalAddress(void* virtualAddress) const override;

    virtual void Flush(void* virtualAddress, uint64_t pageCount) const override;

    virtual void* GetRoot() const override;

    void SetTable(x86_64_Level4Table* table);

private:
    uint32_t DecodePageFlags(PagePermission permission, PageCache cache, PageType type) const;

private:
    x86_64_Level4Table* m_table;
};

#endif /* _x86_64_PAGE_TABLE_HPP */