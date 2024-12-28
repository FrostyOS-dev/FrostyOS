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

x86_64_IDTEntry g_x86_64_IDT[256];

void x86_64_InitIDT() {
    memset(g_x86_64_IDT, 0, sizeof(g_x86_64_IDT));

    for (int i = 0; i < 256; i++) {
        g_x86_64_IDT[i].Selector = x86_64_GDT_KERNEL_CODE_SEGMENT;
        g_x86_64_IDT[i].IST = 0;
        g_x86_64_IDT[i].Type = (uint8_t)x86_64_IDTGateType::Interrupt;
        g_x86_64_IDT[i].DPL = 0;
        g_x86_64_IDT[i].Present = 1;
    }

    x86_64_InitISRs();

    x86_64_IDTPointer idtPointer;
    idtPointer.Limit = sizeof(g_x86_64_IDT) - 1;
    idtPointer.Base = (uint64_t)&g_x86_64_IDT;
    x86_64_LoadIDT(&idtPointer);
}

void x86_64_IDT_SetHandler(uint8_t vector, void (*handler)()) {
    uint64_t handlerAddress = (uint64_t)handler;
    g_x86_64_IDT[vector].Offset0 = handlerAddress & 0xFFFF;
    g_x86_64_IDT[vector].Offset1 = (handlerAddress >> 16) & 0xFFFF;
    g_x86_64_IDT[vector].Offset2 = (handlerAddress >> 32) & 0xFFFFFFFF;
}
