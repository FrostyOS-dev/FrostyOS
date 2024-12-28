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

#ifndef _x86_64_GDT_HPP
#define _x86_64_GDT_HPP

#include <stdint.h>

#define x86_64_GDT_KERNEL_CODE_SEGMENT 0x8
#define x86_64_GDT_KERNEL_DATA_SEGMENT 0x10

enum class x86_64_GDTAccess : uint8_t {
    Accessed = 1 << 0,
    ReadWrite = 1 << 1,
    Direction = 1 << 2,
    Execute = 1 << 3,
    System = 0 << 4,
    NonSystem = 1 << 4,
    Privilege0 = 0 << 5,
    Privilege1 = 1 << 5,
    Privilege2 = 2 << 5,
    Privilege3 = 3 << 5,
    Present = 1 << 7
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

struct [[gnu::packed]] x86_64_GDTPointer {
    uint16_t Limit;
    uint64_t Base;
};

void x86_64_InitGDT();

extern "C" void x86_64_LoadGDT(x86_64_GDTPointer* gdtPointer, uint16_t codeSegment, uint16_t dataSegment);

#endif /* _x86_64_GDT_HPP */