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

x86_64_GDTEntry g_x86_64_GDT[5];

void x86_64_InitGDT() {
    memset(g_x86_64_GDT, 0, sizeof(g_x86_64_GDT));

    g_x86_64_GDT[1].Limit0 = 0xFFFF;
    g_x86_64_GDT[1].Base0 = 0;
    g_x86_64_GDT[1].Base1 = 0;
    g_x86_64_GDT[1].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::Execute | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege0 | (uint8_t)x86_64_GDTAccess::Present;
    g_x86_64_GDT[1].Limit1 = 0xF;
    g_x86_64_GDT[1].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    g_x86_64_GDT[1].Base2 = 0;

    g_x86_64_GDT[2].Limit0 = 0xFFFF;
    g_x86_64_GDT[2].Base0 = 0;
    g_x86_64_GDT[2].Base1 = 0;
    g_x86_64_GDT[2].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege0 | (uint8_t)x86_64_GDTAccess::Present;
    g_x86_64_GDT[2].Limit1 = 0xF;
    g_x86_64_GDT[2].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    g_x86_64_GDT[2].Base2 = 0;

    g_x86_64_GDT[3].Limit0 = 0xFFFF;
    g_x86_64_GDT[3].Base0 = 0;
    g_x86_64_GDT[3].Base1 = 0;
    g_x86_64_GDT[3].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::Execute | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege3 | (uint8_t)x86_64_GDTAccess::Present;
    g_x86_64_GDT[3].Limit1 = 0xF;
    g_x86_64_GDT[3].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    g_x86_64_GDT[3].Base2 = 0;

    g_x86_64_GDT[4].Limit0 = 0xFFFF;
    g_x86_64_GDT[4].Base0 = 0;
    g_x86_64_GDT[4].Base1 = 0;
    g_x86_64_GDT[4].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege3 | (uint8_t)x86_64_GDTAccess::Present;
    g_x86_64_GDT[4].Limit1 = 0xF;
    g_x86_64_GDT[4].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    g_x86_64_GDT[4].Base2 = 0;

    x86_64_GDTPointer gdtPointer;
    gdtPointer.Limit = sizeof(g_x86_64_GDT) - 1;
    gdtPointer.Base = (uint64_t)&g_x86_64_GDT;

    x86_64_LoadGDT(&gdtPointer, 0x8, 0x10);
}