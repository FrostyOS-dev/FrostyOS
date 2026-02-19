/*
Copyright (©) 2025-2026  Frosty515

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

#ifndef _PAGE_MAPPER_HPP
#define _PAGE_MAPPER_HPP

#include <stdint.h>

#include "VMM.hpp"

class PageMapper {
public:
    virtual ~PageMapper() {}

    virtual bool MapPage(uint64_t virt, uint64_t phys, VMM::Protection prot, VMM::CacheType cacheType) = 0;
    virtual bool MapPages(uint64_t virt, uint64_t phys, size_t count, VMM::Protection prot, VMM::CacheType cacheType) = 0;
    virtual bool UnmapPage(uint64_t virt) = 0;
    virtual bool UnmapPages(uint64_t virt, size_t count) = 0;
    virtual bool RemapPage(uint64_t virt, VMM::Protection prot, VMM::CacheType cacheType) = 0;
    virtual bool RemapPages(uint64_t virt, size_t count, VMM::Protection prot, VMM::CacheType cacheType) = 0;

    virtual void InvalidatePages(uint64_t virt, size_t count) = 0;
};

extern PageMapper* g_KPageMapper;

#endif /* _PAGE_MAPPER_HPP */