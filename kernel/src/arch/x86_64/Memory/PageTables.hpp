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

#ifndef _x86_64_PAGE_TABLES_HPP
#define _x86_64_PAGE_TABLES_HPP

#include <stdint.h>

struct x86_64_CR3 {
    uint8_t ignored0 : 3;
    uint8_t PWT : 1;
    uint8_t PCD : 1;
    uint8_t ignored1 : 7;
    uint64_t PML4 : 40;
    uint16_t reserved : 12;
} __attribute__((packed));

struct x86_64_PML4Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t ignored0 : 1;
    uint8_t Reserved : 1;
    uint8_t ignored1 : 4;
    uint64_t Base : 40;
    uint16_t ignored2 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_PML3Entry_1GB {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PageSize : 1; // Must be 1
    uint8_t Global : 1;
    uint8_t ignored0 : 3;
    uint8_t PAT : 1;
    uint32_t Reserved : 17;
    uint64_t Base : 22;
    uint16_t ignored1 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_PML3Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t ignored0 : 1;
    uint8_t PageSize : 1; // Must be 0
    uint8_t ignored1 : 4;
    uint64_t Base : 40;
    uint16_t ignored2 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_PML2Entry_2MB {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PageSize : 1; // Must be 1
    uint8_t Global : 1;
    uint8_t ignored0 : 3;
    uint8_t PAT : 1;
    uint32_t Reserved : 8;
    uint64_t Base : 31;
    uint16_t ignored1 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_PML2Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t ignored0 : 1;
    uint8_t PageSize : 1; // Must be 0
    uint8_t ignored1 : 4;
    uint64_t Base : 40;
    uint16_t ignored2 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_PML1Entry {
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisable : 1;
    uint8_t Accessed : 1;
    uint8_t Dirty : 1;
    uint8_t PAT : 1; // Must be 0
    uint8_t Global : 1;
    uint8_t ignored0 : 3;
    uint64_t Base : 40;
    uint16_t ignored1 : 11;
    uint8_t ExecuteDisable : 1;
} __attribute__((packed));

struct x86_64_Level4Table {
    x86_64_PML4Entry entries[512];
} __attribute__((packed));

struct x86_64_Level3Table {
    union {
        x86_64_PML3Entry entries[512];
        x86_64_PML3Entry_1GB entries_1GB[512];
    } __attribute__((packed));
} __attribute__((packed));

struct x86_64_Level2Table {
    union {
        x86_64_PML2Entry entries[512];
        x86_64_PML2Entry_2MB entries_2MB[512];
    } __attribute__((packed));
} __attribute__((packed));

struct x86_64_Level1Table {
    x86_64_PML1Entry entries[512];
} __attribute__((packed));

uint64_t x86_64_GetPhysicalAddress(x86_64_Level4Table* Table, uint64_t VirtualAddress);

/* mapping flags for standard mappings are broken up into multiple parts:
 * 0-11: flags
 * 12-15: ignored
 * 16-27: high flags
 * 28-31: ignored
 */

void x86_64_MapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags);
void x86_64_RemapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags);
void x86_64_UnmapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress);

/* mapping flags for 2MiB and 1GiB mappings are broken up into multiple parts:
 * 0-12: flags
 * 13-15: ignored
 * 16-27: high flags
 * 28-31: ignored
 */

void x86_64_Map2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags);
void x86_64_Remap2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags);
void x86_64_Unmap2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress);

void x86_64_Map1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags);
void x86_64_Remap1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags);
void x86_64_Unmap1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress);

extern "C" void x86_64_InvalidatePage(x86_64_Level4Table* Table, uint64_t VirtualAddress);
extern "C" void x86_64_FullTLBFlush();

extern "C" void x86_64_LoadCR3(x86_64_CR3 cr3);
extern "C" x86_64_CR3 x86_64_GetCR3();

extern x86_64_Level4Table* g_Level4Table;

#endif /* _x86_64_PAGE_TABLES_HPP */