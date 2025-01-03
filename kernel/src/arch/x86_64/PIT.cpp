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

#include "IO.h"
#include "PIT.hpp"

#include "interrupts/IRQ.hpp"
#include "interrupts/ISR.hpp"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

#define PIT_COMMAND_CHANNEL0 0
#define PIT_COMMAND_READ_BACK 0xC0

#define PIT_COMMAND_ACCESS_LO 0x10
#define PIT_COMMAND_ACCESS_HI 0x20
#define PIT_COMMAND_ACCESS_LOHI 0x30

#define PIT_COMMAND_MODE0 0
#define PIT_COMMAND_MODE1 0x2
#define PIT_COMMAND_MODE2 0x4
#define PIT_COMMAND_MODE3 0x6
#define PIT_COMMAND_MODE4 0x8
#define PIT_COMMAND_MODE5 0xA

#define PIT_COMMAND_BINARY 0
#define PIT_COMMAND_BCD 1

uint64_t g_x86_64_PITTicks = 0; // in ms

void x86_64_PIT_Handler(x86_64_ISR_Frame* frame, uint8_t irq) {
    g_x86_64_PITTicks += 5;
}

void x86_64_PIT_Init() {
    x86_64_outb(PIT_COMMAND, PIT_COMMAND_CHANNEL0 | PIT_COMMAND_ACCESS_LOHI | PIT_COMMAND_MODE3 | PIT_COMMAND_BINARY);

    x86_64_IRQ_RegisterHandler(0, x86_64_PIT_Handler);

    x86_64_PIT_SetDivisor(5966); // just under 200 Hz
}

void x86_64_PIT_SetDivisor(uint16_t divisor) {
    x86_64_outb(PIT_CHANNEL0, divisor & 0xFF);
    x86_64_outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

uint64_t x86_64_PIT_GetTicks() {
    return g_x86_64_PITTicks;
}

void x86_64_PIT_SetTicks(uint64_t ticks) {
    g_x86_64_PITTicks = ticks;
}
