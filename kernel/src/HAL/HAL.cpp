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

#include "ACPI/FADT.hpp"
#include "ACPI/Init.hpp"

#include "drivers/BasicDevices/NullDevice.hpp"
#include "drivers/BasicDevices/ZeroDevice.hpp"

#include "drivers/Input/PS2/PS2Controller.hpp"

#include "drivers/Input/InputEvent.hpp"

#include "drivers/Video/VideoDevice.hpp"

#include "drivers/DeviceManager.hpp"
#include "drivers/PCI.hpp"

#ifdef __x86_64__
#include <arch/x86_64/interrupts/IRQ.hpp>

#include <arch/x86_64/interrupts/APIC/IOAPIC.hpp>
#endif

#include <tty/TTY.hpp>

void HAL_EarlyInit(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical, void* RSDP) {
    g_BSP->Init(HHDMOffset, memoryMap, memoryMapEntryCount, pagingMode, kernelVirtual, kernelPhysical);
    ACPI::EarlyInit(RSDP);
    HAL_InitTime();
    g_BSP->InitBSPLate();
}

void HAL_Stage2() {
    ACPI::Stage2Init();
    g_BSP->InitStage2();

    PCI_Walk();

    g_DeviceManager->ProbeDevices();

    if (FADT_DoesPS2ControllerExist()) {
        g_PS2Controller = new PS2Controller;
        g_DeviceManager->AddDevice(g_PS2Controller);
    }
}

void HAL_InitialseDevices(FBConsole** newConsole) {
    g_NullDevice = new NullDevice;
    if (g_NullDevice == nullptr || 0 != g_DeviceManager->AddDevice(g_NullDevice)) {
        PANIC("Null device creation failed!");
    }

    g_ZeroDevice = new ZeroDevice;
    if (g_ZeroDevice == nullptr || 0 != g_DeviceManager->AddDevice(g_ZeroDevice)) {
        PANIC("Zero device creation failed!");
    }

    {
        using namespace Input;
        EventManager* mgr = new EventManager;
        if (!mgr->Init() || g_EventManager == nullptr)
            PANIC("Input EventManager init failed!");
    }

    g_DeviceManager->AddDevice(g_KTTY);

    g_DeviceManager->InitDevices();

    Input::g_EventManager->FullInit();

    Video::Video_FullInit(newConsole);

    Input::InitTTYEvents();
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
        handler->handler(handler->ctx);
}

void* HAL_RegisterIntHandler(uint32_t GSI, GSIHandler_t handler, void* ctx, bool unmask) {
    HAL_IntHandlerData* data = new HAL_IntHandlerData{GSI, handler, ctx};
    if (x86_64_RegisterGSIHandler(GSI, HAL_GSIHandlerWrapper, data)) {
        if (unmask)
            x86_64_UnmaskGSI(GSI);
        return data;
    }
    delete data;
    return nullptr;
}

void* HAL_RegisterLegacyIntHandler(uint8_t irq, GSIHandler_t handler, void* ctx, bool unmask) {
    return HAL_RegisterIntHandler(x86_64_GetGSIFromSource(irq), handler, ctx, unmask);
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

#endif
