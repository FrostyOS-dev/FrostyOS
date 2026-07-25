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

#ifndef _x86_64_PAGE_MAPPER_HPP
#define _x86_64_PAGE_MAPPER_HPP

#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>

#include <Memory/PageMapper.hpp>

class x86_64_PageMapper : public PageMapper {
public:
    x86_64_PageMapper();
    x86_64_PageMapper(void* pageTable);
    ~x86_64_PageMapper();

    bool MapPage(uint64_t virt, uint64_t phys, VMM::Protection prot, bool user, VMM::CacheType cacheType) override;
    bool MapPages(uint64_t virt, uint64_t phys, size_t count, VMM::Protection prot, bool user, VMM::CacheType cacheType) override;
    bool UnmapPage(uint64_t virt) override;
    bool UnmapPages(uint64_t virt, size_t count) override;
    bool RemapPage(uint64_t virt, VMM::Protection prot, bool user, VMM::CacheType cacheType) override;
    bool RemapPages(uint64_t virt, size_t count, VMM::Protection prot, bool user, VMM::CacheType cacheType) override;

    uint64_t GetPhysicalAddr(uint64_t virt) override;

    void InvalidatePages(uint64_t virt, size_t count, bool shootdown) override;

    void SetPageTable(void* pageTable);
    void* GetPageTable() const override;

    bool isPermsReduction(VMM::Protection oldProt, VMM::Protection newProt) const override;

    void Create() override;
    void Delete() override;

    bool SwapToThis() override;

private:
    void* m_pageTable;
    spinlock_t m_lock;
};

PageMapper* CreatePageMapper();

#endif /* _x86_64_PAGE_MAPPER_HPP */