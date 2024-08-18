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

struct GDTR {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct GDTSegmentDescriptor {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1 : 4;
    uint8_t flags : 4;
    uint8_t base2;
} __attribute__((packed));

enum GDTAccess {
    GDTAccess_Accessed = 1 << 0,
    GDTAccess_ReadWrite = 1 << 1,
    GDTAccess_Direction = 1 << 2,
    GDTAccess_Execute = 1 << 3,
    GDTAccess_CodeData = 1 << 4,
    GDTAccess_RING0 = 0,
    GDTAccess_RING1 = 1 << 5,
    GDTAccess_RING2 = 1 << 6,
    GDTAccess_RING3 = (1 << 5) | (1 << 6),
    GDTAccess_Present = 1 << 7
};

enum GDTFlags {
    GDTFlags_Granularity = 1 << 3,
    GDTFlags_32Bit = 1 << 2,
    GDTFlags_LongMode = 1 << 1
};

struct GDTSystemDescriptor {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1 : 4;
    uint8_t flags : 4;
    uint8_t base2;
    uint32_t base3;
    uint32_t reserved;
} __attribute__((packed));

extern GDTSegmentDescriptor g_GDT[5]; // reserved, code, data, user code, user data

void x86_64_GDTInit(GDTSegmentDescriptor* gdt);

extern "C" void x86_64_LoadGDT(GDTR* gdtr);

#endif /* _x86_64_GDT_HPP */