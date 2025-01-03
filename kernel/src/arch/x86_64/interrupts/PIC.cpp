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

#include "PIC.hpp"

#include "../IO.h"

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xA0
#define PIC_SLAVE_DATA 0xA1

#define PIC_EOI 0x20

#define PIC_ICW1_ICW4 0x01
#define PIC_ICW1_SINGLE 0x02
#define PIC_ICW1_INTERVAL4 0x04
#define PIC_ICW1_LEVEL 0x08
#define PIC_ICW1_INIT 0x10

#define PIC_ICW4_8086 0x01
#define PIC_ICW4_AUTO 0x02
#define PIC_ICW4_BUF_SLAVE 0x08
#define PIC_ICW4_BUF_MASTER 0x0C
#define PIC_ICW4_SFNM 0x10

uint16_t g_x86_64_CurrentPICMask = 0xFFFF;

void x86_64_PIC_Init() {
    x86_64_outb(PIC_MASTER_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    x86_64_IOWait();
    x86_64_outb(PIC_SLAVE_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    x86_64_IOWait();

    x86_64_outb(PIC_MASTER_DATA, x86_64_PIC_REMAP_OFFSET);
    x86_64_IOWait();
    x86_64_outb(PIC_SLAVE_DATA, x86_64_PIC_REMAP_OFFSET + 8);
    x86_64_IOWait();

    x86_64_outb(PIC_MASTER_DATA, 4); // slave PIC at IRQ2
    x86_64_IOWait();
    x86_64_outb(PIC_SLAVE_DATA, 2); // cascade identity
    x86_64_IOWait();

    x86_64_outb(PIC_MASTER_DATA, PIC_ICW4_8086);
    x86_64_IOWait();
    x86_64_outb(PIC_SLAVE_DATA, PIC_ICW4_8086);
    x86_64_IOWait();

    x86_64_PIC_MaskAll();
}

void x86_64_PIC_SendEOI(uint8_t irq) {
    if (irq >= 8)
        x86_64_outb(PIC_SLAVE_COMMAND, PIC_EOI);
    x86_64_outb(PIC_MASTER_COMMAND, PIC_EOI);
}

void x86_64_PIC_MaskIRQ(uint8_t irq) {
    g_x86_64_CurrentPICMask |= (1 << irq);
    if (irq < 8)
        x86_64_outb(PIC_MASTER_DATA, g_x86_64_CurrentPICMask & 0xFF);
    else
        x86_64_outb(PIC_SLAVE_DATA, (g_x86_64_CurrentPICMask >> 8) & 0xFF);
}

void x86_64_PIC_UnmaskIRQ(uint8_t irq) {
    g_x86_64_CurrentPICMask &= ~(1 << irq);
    if (irq < 8)
        x86_64_outb(PIC_MASTER_DATA, g_x86_64_CurrentPICMask & 0xFF);
    else
        x86_64_outb(PIC_SLAVE_DATA, (g_x86_64_CurrentPICMask >> 8) & 0xFF);
}

void x86_64_PIC_MaskAll() {
    x86_64_outb(PIC_MASTER_DATA, 0xFF);
    x86_64_outb(PIC_SLAVE_DATA, 0xFF);
    g_x86_64_CurrentPICMask = 0xFFFF;
}
