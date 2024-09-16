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

#include "PIC.hpp"

#include "../ArchDefs.h"
#include "../IO.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM 0x10

#define PIC_EOI 0x20

void x86_64_InitPIC() {
    x86_64_DisableInterrupts();
    // ICW1: Start initialization
    x86_64_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    x86_64_IOWait();
    x86_64_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    x86_64_IOWait();

    // ICW2: Set interrupt vectors
    x86_64_outb(PIC1_DATA, 0x20);
    x86_64_IOWait();
    x86_64_outb(PIC2_DATA, 0x28);
    x86_64_IOWait();

    // ICW3: Set up cascading
    x86_64_outb(PIC1_DATA, 4);
    x86_64_IOWait();
    x86_64_outb(PIC2_DATA, 2);
    x86_64_IOWait();

    // ICW4: Set up mode
    x86_64_outb(PIC1_DATA, ICW4_8086);
    x86_64_IOWait();
    x86_64_outb(PIC2_DATA, ICW4_8086);
    x86_64_IOWait();

    // Mask all IRQs
    x86_64_outb(PIC1_DATA, 0xFF);
    x86_64_outb(PIC2_DATA, 0xFF);
    x86_64_EnableInterrupts();
}

void x86_64_PIC_SendEOI(uint8_t irq) {
    if (irq >= 8) {
        x86_64_outb(PIC2_COMMAND, PIC_EOI);
    }
    x86_64_outb(PIC1_COMMAND, PIC_EOI);
}

void x86_64_PIC_MaskIRQ(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8)
        port = PIC1_DATA;
    else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = x86_64_inb(port) | (1 << irq);
    x86_64_outb(port, value);
}

void x86_64_PIC_UnmaskIRQ(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8)
        port = PIC1_DATA;
    else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = x86_64_inb(port) & ~(1 << irq);
    x86_64_outb(port, value);
}