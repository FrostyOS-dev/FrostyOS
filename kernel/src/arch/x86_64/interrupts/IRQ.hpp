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

#ifndef _x86_64_IRQ_HPP
#define _x86_64_IRQ_HPP

#include "ISR.hpp"

#include <stdint.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/HashMap.hpp>

typedef void (*x86_64_IRQHandler_t)(x86_64_ISR_Frame* frame, uint8_t irq);
typedef void (*x86_64_GSIHandler_t)(x86_64_ISR_Frame* frame, void* ctx);

struct x86_64_ProcessorIRQData {
    struct HandlerData {
        x86_64_GSIHandler_t handler;
        void* ctx;
    };

    RawBitmap usedInterrupts;
    HashMap<uint8_t, HandlerData*> handlerMap;
};

void x86_64_IRQ_EarlyInit(); // called before memory management is ready
void x86_64_IRQ_FullInit(); // called after current processor is ready

void x86_64_PICHandler(x86_64_ISR_Frame* frame);
void x86_64_IRQHandler(x86_64_ISR_Frame* frame);

// Register a handler, including validating the GSI. Cannot be called until I/O APICs are intialised.
bool x86_64_RegisterGSIHandler(uint32_t GSI, x86_64_GSIHandler_t handler, void* ctx);

bool x86_64_RemoveGSIHandler(uint32_t GSI);

bool x86_64_MaskGSI(uint32_t GSI);
bool x86_64_UnmaskGSI(uint32_t GSI);

#endif /* _x86_64_IRQ_HPP */