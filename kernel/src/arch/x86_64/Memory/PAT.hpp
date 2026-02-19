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

#ifndef _x86_64_PAT_HPP
#define _x86_64_PAT_HPP

#include <stdint.h>

/*
Layout of the PAT:
    0: Write-Back
    1: Write-Through
    2: Uncached
    3: Uncachable
    4: Write-Protected
    5: Write-Combining
    6: Reserved
    7: Reserved
*/

enum class x86_64_PATOffset {
    Default = 0,
    WriteBack = 0, // WB
    WriteThrough = 1, // WT
    Uncached = 2, // UC-
    Uncachable = 3, // UC
    WriteProtected = 4, // WP
    WriteCombining = 5 // WC
};

enum class x86_64_PATEncoding {
    Uncachable = 0,
    WriteCombining = 1,
    WriteThrough = 4,
    WriteProtected = 5,
    WriteBack = 6,
    Uncached = 7
};

void x86_64_InitPAT();
bool x86_64_IsPATSupported();

uint16_t x86_64_PAT_GetPageMappingFlags(x86_64_PATOffset offset);
uint16_t x86_64_PAT_GetLargePageMappingFlags(x86_64_PATOffset offset);

#endif /* _x86_64_PAT_HPP */