/*
Copyright (©) 2025  Frosty515

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

#include "PAT.hpp"

#include "../CPUID.h"
#include "../MSR.h"

void x86_64_InitPAT() {
    uint64_t PAT = 0;
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteBack << 0);
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteThrough << 8);
    PAT |= ((uint64_t)x86_64_PATEncoding::Uncached << 16);
    PAT |= ((uint64_t)x86_64_PATEncoding::Uncachable << 24);
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteProtected << 32);
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteCombining << 40);
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteBack << 48); // set the default
    PAT |= ((uint64_t)x86_64_PATEncoding::WriteBack << 56); // set the default

    x86_64_WriteMSR(0x277, PAT);
}

bool x86_64_IsPATSupported() {
    x86_64_CPUIDResult result = x86_64_CPUID(1, 0);
    return result.EDX & (1 << 16);
}

uint16_t x86_64_PAT_GetPageMappingFlags(x86_64_PATOffset offset) {
    uint16_t raw_offset = (uint16_t)offset;
    // raw offset bits 0-1 are moved to bits 3-4, raw offset bit 2 is moved to bit 7
    return (raw_offset & 0b11) << (3 - 0) | (raw_offset & 0b100) << (7 - 2);
}

uint16_t x86_64_PAT_GetLargePageMappingFlags(x86_64_PATOffset offset) {
    uint16_t raw_offset = (uint16_t)offset;
    // raw offset bits 0-1 are moved to bits 3-4, raw offset bit 2 is moved to bit 12
    return (raw_offset & 0b11) << (3 - 0) | (raw_offset & 0b100) << (12 - 2);
}
