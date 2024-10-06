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

#ifndef _PAGE_TABLE_HPP
#define _PAGE_TABLE_HPP

#include <stdint.h>

enum class PagePermission {
    READ,
    WRITE,
    READ_WRITE,
    READ_EXECUTE
};

enum class PageCache {
    WRITE_BACK,
    WRITE_THROUGH,
    WRITE_COMBINING,
    WRITE_PROTECTED,
    UNCACHED,
    UNCACHED_WEAK,
    DEFAULT = WRITE_BACK
};

enum class PageType {
    NORMAL,
    LARGE,
    HUGE
};

class PageTable {
public:
    PageTable(bool user = false);
    virtual ~PageTable();

    virtual void Map(void* virtualAddress, void* physicalAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT, bool flush = true, PageType type = PageType::NORMAL);
    virtual void Remap(void* virtualAddress, PagePermission permission = PagePermission::READ_WRITE, PageCache cache = PageCache::DEFAULT, bool flush = true, PageType type = PageType::NORMAL);
    virtual void Unmap(void* virtualAddress, bool flush = true, PageType type = PageType::NORMAL);
    virtual void* GetPhysicalAddress(void* virtualAddress) const;

    virtual void Flush(void* virtualAddress, uint64_t pageCount) const;

    virtual void* GetRoot() const;

    bool IsUser() const;

private:
    bool m_user;
};

extern PageTable* g_KPageTable;


#endif /* _PAGE_TABLE_HPP */