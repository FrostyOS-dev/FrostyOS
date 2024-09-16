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
#include "Processor.hpp"

#include "interrupts/IDT.hpp"
#include "interrupts/PIC.hpp"

#include <string.h>

Processor* g_BSP = nullptr;

Processor::Processor(bool BSP) : m_BSP(BSP), m_LAPIC(nullptr), m_GDT() {
    memset(m_GDT, 0, sizeof(m_GDT));
}

Processor::~Processor() {

}

void Processor::Init(x86_64_LAPIC* LAPIC) {
    x86_64_GDTInit(m_GDT);
    if (m_BSP) {
        x86_64_IDTInit();
        x86_64_InitPIC();
    }
    else {
        IDTR idtr;
        idtr.limit = sizeof(IDTGateDescriptor) * 256 - 1;
        idtr.base = (uint64_t)g_IDT;
        x86_64_LoadIDT(&idtr);

        InitLAPIC(LAPIC);
    }
}

void Processor::InitLAPIC(x86_64_LAPIC* LAPIC) {
    m_LAPIC = LAPIC;
    m_LAPIC->Init();
}