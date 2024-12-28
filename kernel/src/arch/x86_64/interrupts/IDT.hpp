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

enum class x86_64_IDTGateType : uint8_t {
    Interrupt = 0xE,
    Trap = 0xF
};

struct [[gnu::packed]] x86_64_IDTEntry {
    uint16_t Offset0;
    uint16_t Selector;
    uint8_t IST : 3;
    uint8_t Reserved0 : 5;
    uint8_t Type : 4;
    uint8_t Zero : 1;
    uint8_t DPL : 2;
    uint8_t Present : 1;
    uint16_t Offset1;
    uint32_t Offset2;
    uint32_t Reserved1;
};

struct [[gnu::packed]] x86_64_IDTPointer {
    uint16_t Limit;
    uint64_t Base;
};

extern x86_64_IDTEntry g_x86_64_IDT[256];

void x86_64_InitIDT();
void x86_64_IDT_SetHandler(uint8_t vector, void (*handler)());

extern "C" void x86_64_LoadIDT(x86_64_IDTPointer* pointer);

#endif /* _x86_64_IDT_HPP */