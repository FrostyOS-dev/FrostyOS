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

#include "PAT.hpp"

#include "../MSR.h"

#include <string.h>

bool g_PATSupported = false;
bool g_PATChecked = false;

void x86_64_InitPAT() {
    x86_64_PAT pat;
    memset(&pat, 0, sizeof(x86_64_PAT));
    pat.PAT0 = (int)x86_64_PATType::WriteBack;
    pat.PAT1 = (int)x86_64_PATType::WriteThrough;
    pat.PAT2 = (int)x86_64_PATType::Uncached;
    pat.PAT3 = (int)x86_64_PATType::Uncacheable;
    pat.PAT4 = (int)x86_64_PATType::WriteProtected;
    pat.PAT5 = (int)x86_64_PATType::WriteCombining;
    pat.PAT6 = 0;
    pat.PAT7 = 0;

    uint64_t* patPtr = (uint64_t*)&pat;

    x86_64_WriteMSR(0x277, *patPtr);
}

uint8_t x86_64_GetPATIndex(x86_64_PATType type) {
    switch (type) {
        case x86_64_PATType::WriteBack:
            return 0;
        case x86_64_PATType::WriteThrough:
            return 1;
        case x86_64_PATType::Uncached:
            return 2;
        case x86_64_PATType::Uncacheable:
            return 3;
        case x86_64_PATType::WriteProtected:
            return 4;
        case x86_64_PATType::WriteCombining:
            return 5;
        default:
            return 0;
    }
}

uint32_t x86_64_GetPageFlagsFromPATIndex(uint8_t index) {
    return (index & 3) << 3 | (index & 4) << (7 - 2);
}

uint32_t x86_64_GetLargePageFlagsFromPATIndex(uint8_t index) {
    return (index & 3) << 3 | (index & 4) << (12 - 2);
}

bool x86_64_isPATSupported() {
    if (!g_PATChecked) {
        g_PATSupported = x86_64_EnsurePAT();
        g_PATChecked = true;
    }
    return g_PATSupported;
}
