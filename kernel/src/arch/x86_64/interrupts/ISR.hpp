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

#ifndef _x86_64_ISR_HPP
#define _x86_64_ISR_HPP

#include "../ArchDefs.h"

struct x86_64_ISR_Frame {
    uint64_t DS;
    uint64_t CR3, CR2;
    uint64_t R15, R14, R13, R12, R11, R10, R9, R8, RDI, RSI, RBP, RBX, RDX, RCX, RAX;
    uint64_t INT, ERR;
    uint64_t RIP, CS, RFLAGS, RSP, SS;
} __attribute__((packed));

void x86_64_Convert_ISRRegs_To_StandardRegs(x86_64_ISR_Frame* frame, x86_64_Registers* state);

typedef void (*x86_64_ISRHandler_t)(x86_64_ISR_Frame* frame);

void x86_64_InitISRs();
void x86_64_ISR_RegisterHandler(uint8_t vector, x86_64_ISRHandler_t handler);

extern "C" void x86_64_ISR_Handler(x86_64_ISR_Frame* frame);

#endif /* _x86_64_ISR_HPP */