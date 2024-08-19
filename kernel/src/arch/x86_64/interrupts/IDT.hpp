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

#ifndef _x86_64_IDT_HPP
#define _x86_64_IDT_HPP

#include <stdint.h>

struct IDTR {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct IDTGateDescriptor {
    uint16_t offset0;
    uint16_t selector;
    uint8_t ist : 3;
    uint8_t reserved0 : 5;
    uint8_t type : 4;
    uint8_t zero0 : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t reserved1;
} __attribute__((packed));

enum GateType {
    INTERRUPT_GATE = 0xE,
    TRAP_GATE = 0xF,
    TASK_GATE = 0x5
};

extern IDTGateDescriptor g_IDT[256];

void x86_64_IDTInit();
void x86_64_IDT_SetHandler(uint8_t vector, void (*handler)());

extern "C" void x86_64_LoadIDT(IDTR* idtr);

#endif /* _x86_64_IDT_HPP */