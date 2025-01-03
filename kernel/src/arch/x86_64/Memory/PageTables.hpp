/*
Copyright (©) 2024-2025  Frosty515

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

#ifndef _x86_64_PAGE_TABLES_HPP
#define _x86_64_PAGE_TABLES_HPP

#include <stdint.h>

struct [[gnu::packed]] x86_64_PML5Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Ignored0 : 1;
    uint8_t Reserved0 : 1;
    uint8_t Ignored1 : 4;
    uint64_t Address : 40;
    uint16_t Ignored2 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML4Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Ignored0 : 1;
    uint8_t Reserved0 : 1;
    uint8_t Ignored1 : 4;
    uint64_t Address : 40;
    uint16_t Ignored2 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML3Entry_1GiBPages {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PageSize : 1; // Must be 1 for 1 GiB pages
    uint8_t Global : 1;
    uint8_t Ignored0 : 3;
    uint8_t PAT : 1;
    uint32_t Reserved0 : 17;
    uint64_t Address : 22;
    uint16_t Ignored1 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML3Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Ignored0 : 1;
    uint8_t PageSize : 1; // Must be 0 for non-1 GiB pages
    uint8_t Ignored1 : 4;
    uint64_t Address : 40;
    uint16_t Ignored2 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML2Entry_2MiBPages {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PageSize : 1; // Must be 1 for 2 MiB pages
    uint8_t Global : 1;
    uint8_t Ignored0 : 3;
    uint8_t PAT : 1;
    uint32_t Reserved0 : 8;
    uint64_t Address : 31;
    uint16_t Ignored1 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML2Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Ignored0 : 1;
    uint8_t PageSize : 1; // Must be 0 for non-2 MiB pages
    uint8_t Ignored1 : 4;
    uint64_t Address : 40;
    uint16_t Ignored2 : 11;
    uint8_t ExecuteDisable : 1;
};

struct [[gnu::packed]] x86_64_PML1Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSupervisor : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PAT : 1;
    uint8_t Global : 1;
    uint8_t Ignored0 : 3;
    uint64_t Address : 40;
    uint16_t Ignored1 : 11;
    uint8_t ExecuteDisable : 1;
};

void* x86_64_GetNextPageTable(void* pageTableEntry, uint64_t index);

uint64_t x86_64_GetPhysicalAddress(void* pageTable, uint64_t virtualAddress);

void x86_64_MapPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags);
void x86_64_RemapPage(void* pageTable, uint64_t virtualAddress, uint32_t flags);
void x86_64_UnmapPage(void* pageTable, uint64_t virtualAddress);

void x86_64_Map2MiBPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags);
void x86_64_Remap2MiBPage(void* pageTable, uint64_t virtualAddress, uint32_t flags);
void x86_64_Unmap2MiBPage(void* pageTable, uint64_t virtualAddress);

void x86_64_Map1GiBPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags);
void x86_64_Remap1GiBPage(void* pageTable, uint64_t virtualAddress, uint32_t flags);
void x86_64_Unmap1GiBPage(void* pageTable, uint64_t virtualAddress);

#endif /* _x86_64_PAGE_TABLES_HPP */