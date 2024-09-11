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

#ifndef _x86_64_PAT_HPP
#define _x86_64_PAT_HPP

#include <stdint.h>

struct x86_64_PAT {
#define PAT_ENTRY(x) uint8_t PAT##x : 3; uint8_t Reserved##x : 5;
    PAT_ENTRY(0)
    PAT_ENTRY(1)
    PAT_ENTRY(2)
    PAT_ENTRY(3)
    PAT_ENTRY(4)
    PAT_ENTRY(5)
    PAT_ENTRY(6)
    PAT_ENTRY(7)
#undef PAT_ENTRY
} __attribute__((packed));

enum class x86_64_PATType {
    Uncacheable = 0,
    WriteCombining = 1,
    WriteThrough = 4,
    WriteProtected = 5,
    WriteBack = 6,
    Uncached = 7,
    Default = WriteBack
};

void x86_64_InitPAT();
uint8_t x86_64_GetPATIndex(x86_64_PATType type);

uint32_t x86_64_GetPageFlagsFromPATIndex(uint8_t index);
uint32_t x86_64_GetLargePageFlagsFromPATIndex(uint8_t index);

bool x86_64_isPATSupported();

extern "C" bool x86_64_EnsurePAT();

#endif /* _x86_64_PAT_HPP */