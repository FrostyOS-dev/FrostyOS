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

#include "IRQ.hpp"
#include "ISR.hpp"
#include "PIC.hpp"

x86_64_IRQHandler_t g_x86_64_IRQHandlers[16];

void x86_64_IRQ_Init() {
    for (uint8_t i = 0; i < 16; i++)
        g_x86_64_IRQHandlers[i] = nullptr;

    x86_64_PIC_Init();

    for (uint8_t i = x86_64_PIC_REMAP_OFFSET; i < x86_64_PIC_REMAP_OFFSET + 16; i++)
        x86_64_ISR_RegisterHandler(i, x86_64_IRQHandler);
}

void x86_64_IRQ_RegisterHandler(uint8_t irq, x86_64_IRQHandler_t handler) {
    g_x86_64_IRQHandlers[irq] = handler;
}

void x86_64_IRQHandler(x86_64_ISR_Frame* frame) {
    uint8_t irq = frame->INT - x86_64_PIC_REMAP_OFFSET;

    if (g_x86_64_IRQHandlers[irq] != nullptr)
        g_x86_64_IRQHandlers[irq](frame, irq);

    x86_64_PIC_SendEOI(irq);
}