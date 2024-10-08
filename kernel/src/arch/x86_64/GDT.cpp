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

#include "GDT.hpp"

#include <string.h>

void x86_64_GDTInit(GDTSegmentDescriptor* gdt) {
    memset(gdt, 0, sizeof(GDTSegmentDescriptor) * 5);

    for (int i = 1; i < 5; i++) {
        gdt[i].limit0 = 0xFFFF;
        gdt[i].base0 = 0;
        gdt[i].base1 = 0;
        gdt[i].limit1 = 0xF;
        gdt[i].flags = GDTFlags_LongMode | GDTFlags_Granularity;
        gdt[i].base2 = 0;
    }

    gdt[1].access = GDTAccess_Present | GDTAccess_ReadWrite | GDTAccess_CodeData | GDTAccess_Execute | GDTAccess_RING0;
    gdt[2].access = GDTAccess_Present | GDTAccess_ReadWrite | GDTAccess_CodeData | GDTAccess_RING0;
    gdt[4].access = GDTAccess_Present | GDTAccess_ReadWrite | GDTAccess_CodeData | GDTAccess_RING3;
    gdt[3].access = GDTAccess_Present | GDTAccess_ReadWrite | GDTAccess_CodeData | GDTAccess_Execute | GDTAccess_RING3;

    GDTR gdtr;
    gdtr.limit = sizeof(GDTSegmentDescriptor) * 5 - 1;
    gdtr.base = (uint64_t)gdt;

    x86_64_LoadGDT(&gdtr);
}

uint16_t x86_64_GetKernelDataSegment() {
    return 0x10;
}

uint16_t x86_64_GetKernelCodeSegment() {
    return 0x08;
}

uint16_t x86_64_GetUserDataSegment() {
    return 0x18 | 3;
}

uint16_t x86_64_GetUserCodeSegment() {
    return 0x20 | 3;
}
