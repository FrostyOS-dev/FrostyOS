/*
Copyright (©) 2024-2026  Frosty515

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

#include "HAL.hpp"
#include "Processor.hpp"
#include "Time.hpp"

#include "ACPI/Init.hpp"

#ifdef __x86_64__
#include <arch/x86_64/interrupts/IRQ.hpp>
#endif

void HAL_EarlyInit(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical, void* RSDP) {
    g_BSP->Init(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
    ACPI::EarlyInit(RSDP);
    HAL_InitTime();
}

void HAL_Stage2() {
    ACPI::Stage2Init();
}

struct HAL_IntHandlerData {
    uint32_t GSI;
    GSIHandler_t handler;
    void* ctx;
};

#ifdef __x86_64__

void HAL_GSIHandlerWrapper(x86_64_ISR_Frame* frame, void* ctx) {
    HAL_IntHandlerData* handler = static_cast<HAL_IntHandlerData*>(ctx);
    if (handler != nullptr)
        handler->handler(ctx);
}

#endif

void* HAL_RegisterIntHandler(uint32_t GSI, GSIHandler_t handler, void* ctx) {
    HAL_IntHandlerData* data = new HAL_IntHandlerData{GSI, handler, ctx};
    if (x86_64_RegisterGSIHandler(GSI, HAL_GSIHandlerWrapper, data))
        return data;
    delete data;
    return nullptr;
}

bool HAL_RemoveIntHandler(void* data) {
    HAL_IntHandlerData* handler = static_cast<HAL_IntHandlerData*>(data);
    if (handler == nullptr)
        return false;
    if (x86_64_RemoveGSIHandler(handler->GSI)) {
        delete handler;
        return true;
    }
    return false;
}
