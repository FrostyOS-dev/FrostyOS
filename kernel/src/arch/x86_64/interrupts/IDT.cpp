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

#include "IDT.hpp"
#include "ISR.hpp"

#include "../GDT.hpp"

#include <string.h>

IDTGateDescriptor g_IDT[256];

void x86_64_IDTInit() {
    memset(g_IDT, 0, sizeof(IDTGateDescriptor) * 256);

    for (int i = 0; i < 256; i++) {
        g_IDT[i].selector = x86_64_GetKernelCodeSegment();
        g_IDT[i].ist = 0;
        g_IDT[i].type = INTERRUPT_GATE;
        g_IDT[i].dpl = 0;
        g_IDT[i].present = 1;
    }

    x86_64_ISRInit();

    IDTR idtr;
    idtr.limit = sizeof(IDTGateDescriptor) * 256 - 1;
    idtr.base = (uint64_t)g_IDT;
    x86_64_LoadIDT(&idtr);
}

void x86_64_IDT_SetHandler(uint8_t vector, void (*handler)()) {
    g_IDT[vector].offset0 = (uint64_t)handler & 0xFFFF;
    g_IDT[vector].offset1 = ((uint64_t)handler >> 16) & 0xFFFF;
    g_IDT[vector].offset2 = ((uint64_t)handler >> 32) & 0xFFFFFFFF;
}
