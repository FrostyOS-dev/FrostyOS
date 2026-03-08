/*
Copyright (©) 2024-2026  Frosty515

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

#ifndef _x86_64_GDT_HPP
#define _x86_64_GDT_HPP

#include <stdint.h>

#define x86_64_GDT_KERNEL_CODE_SEGMENT 0x8
#define x86_64_GDT_KERNEL_DATA_SEGMENT 0x10

#define x86_64_GDT_USER_CODE_SEGMENT 0x18
#define x86_64_GDT_USER_DATA_SEGMENT 0x20

#define x86_64_TSS_SEGMENT 0x28

#define x86_64_GDT_ENTRY_COUNT 7

enum class x86_64_GDTAccess : uint8_t {
    Accessed   = 0x1 << 0,
    ReadWrite  = 0x1 << 1,
    Direction  = 0x1 << 2,
    Execute    = 0x1 << 3,
    System     = 0x0 << 4,
    NonSystem  = 0x1 << 4,
    Privilege0 = 0x0 << 5,
    Privilege1 = 0x1 << 5,
    Privilege2 = 0x2 << 5,
    Privilege3 = 0x3 << 5,
    Present    = 0x1 << 7,
    LDT        = 0x2 << 0,
    TSS_AVAIL  = 0x9 << 0,
    TSS_BUSY   = 0xB << 0
};

enum class x86_64_GDTFlags : uint8_t {
    Granularity = 1 << 3,
    Size = 1 << 2,
    LongMode = 1 << 1,
    Available = 1 << 0
};

struct [[gnu::packed]] x86_64_GDTEntry {
    uint16_t Limit0;
    uint16_t Base0;
    uint8_t Base1;
    uint8_t Access;
    uint8_t Limit1 : 4;
    uint8_t Flags : 4;
    uint8_t Base2;
};

struct [[gnu::packed]] x86_64_GDTSystemEntry {
    uint16_t Limit0;
    uint16_t Base0;
    uint8_t Base1;
    uint8_t Access;
    uint8_t Limit1 : 4;
    uint8_t Flags : 4;
    uint8_t Base2;
    uint32_t Base3;
    uint32_t Reserved;
};

struct [[gnu::packed]] x86_64_GDTPointer {
    uint16_t Limit;
    uint64_t Base;
};

struct [[gnu::packed]] x86_64_TSS {
    uint32_t reserved0;
    uint64_t RSP[3];
    uint64_t reserved1;
    uint64_t IST[7];
    uint16_t reserved2[5];
    uint16_t IOPB;
};

void x86_64_InitGDT(x86_64_GDTEntry* entries, x86_64_TSS* TSS); // entries is assumed to have at least x86_64_GDT_ENTRY_COUNT entries.

extern "C" void x86_64_LoadGDT(x86_64_GDTPointer* gdtPointer, uint16_t codeSegment, uint16_t dataSegment);
extern "C" void x86_64_LoadTSS(uint64_t seg);

#endif /* _x86_64_GDT_HPP */