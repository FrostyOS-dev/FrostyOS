/*
Copyright (©) 2025-2026  Frosty515

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

#include "APIC/IOAPIC.hpp"

#include "../Processor.hpp"

#include <DataStructures/HashMap.hpp>

struct x86_64_IRQInfo {
    x86_64_Processor* proc;
    uint8_t interrupt;
};

HashMap<uint32_t, x86_64_IRQInfo*> g_interruptMap;

void x86_64_IRQ_EarlyInit() {
    x86_64_PIC_Init();

    for (uint8_t i = x86_64_PIC_REMAP_OFFSET; i < x86_64_PIC_REMAP_OFFSET + 16; i++)
        x86_64_ISR_RegisterHandler(i, x86_64_PICHandler);
}

void x86_64_IRQ_FullInit() {
    x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    if (proc == nullptr)
        PANIC("IRQ Init failed: Cannot get current processor");

    x86_64_ProcessorIRQData* data = new x86_64_ProcessorIRQData;
    uint8_t* buffer = static_cast<uint8_t*>(kcalloc(1, 256 / 8));
    for (int i = 0; i < 0x30 / 8; i++) // first 0x20 are reserved, next 0x10 are for the PICs
        buffer[i] = 0xFF;
    buffer[255 / 8] = 0xC0; // highest 2 bits

    data->usedInterrupts.SetBuffer(buffer);
    data->usedInterrupts.SetSize(256 / 8);

    proc->SetIRQData(data);
}

void x86_64_PICHandler(x86_64_ISR_Frame* frame) {
    uint8_t irq = frame->INT - x86_64_PIC_REMAP_OFFSET;

    x86_64_PIC_SendEOI(irq);
}

void x86_64_IRQHandler(x86_64_ISR_Frame* frame) {
    x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    if (proc == nullptr)
        x86_64_Panic("Unable to handle IRQ, CPU is null", frame, true);

    x86_64_LAPIC* lapic = proc->GetLAPIC();
    if (lapic == nullptr)
        x86_64_Panic("Unable to handle IRQ, LAPIC is null", frame, true);

    x86_64_ProcessorIRQData* data = proc->GetIRQData();
    if (data == nullptr) {
        lapic->SendEOI();
        return; // not initialised yet
    }

    x86_64_ProcessorIRQData::HandlerData* handler = data->handlerMap.get(frame->INT);
    if (handler == nullptr) {
        lapic->SendEOI();
        return;
    }

    handler->handler(frame, handler->ctx);

    lapic->SendEOI();
}

bool x86_64_RegisterGSIHandler(uint32_t GSI, x86_64_GSIHandler_t handler, void* ctx) {
    x86_64_IOAPIC* ioapic = x86_64_GetIOAPICForGSI(GSI);
    if (ioapic == nullptr)
        return false; // Invalid GSI or I/O APICs aren't ready

    x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    if (proc == nullptr)
        return false; // no current processor

    // No need for any locking on this as it can only be modified on the current processor
    x86_64_ProcessorIRQData* data = proc->GetIRQData();
    if (data == nullptr)
        return false; // not initialised yet

    for (int i = 0x30; i < 0xFE; i++) {
        if (!data->usedInterrupts.Get(i)) {
            x86_64_LAPIC* lapic = proc->GetLAPIC();
            if (lapic == nullptr)
                return false; // not initialised yet
            
            // fill the redirection entry, ensuring it is masked
            uint64_t index = GSI - ioapic->GetGSIBase();
            x86_64_IOAPIC_RedirectionEntry entry = ioapic->GetRedirectionEntry(index);
            entry.DeliveryMode = static_cast<uint8_t>(x86_64_IOAPIC_DeliveryMode::Fixed);
            entry.Vector = i;
            entry.Destination = lapic->GetID();
            entry.Masked = 1;
            ioapic->SetRedirectionEntry(index, entry);

            // Set the global map entry
            x86_64_IRQInfo* info = new x86_64_IRQInfo;
            info->interrupt = i;
            info->proc = proc;
            g_interruptMap.lock();
            g_interruptMap.insert(GSI, info);
            g_interruptMap.unlock();

            // Set the processor-specific map entry
            x86_64_ProcessorIRQData::HandlerData* handlerData = new x86_64_ProcessorIRQData::HandlerData;
            handlerData->handler = handler;
            handlerData->ctx = ctx;
            data->handlerMap.insert(i, handlerData);

            x86_64_ISR_RegisterHandler(i, x86_64_IRQHandler);
            
            data->usedInterrupts.Set(i, true);
            return true;
        }
    }

    return false; // Didn't find an interrupt
}

bool x86_64_RemoveGSIHandler(uint32_t GSI) {
    g_interruptMap.lock();
    x86_64_IRQInfo* info = g_interruptMap.get(GSI);
    if (info == nullptr) {
        g_interruptMap.unlock();
        return false;
    }

    if (info->proc == nullptr) {
        g_interruptMap.unlock();
        return false;
    }

    x86_64_ProcessorIRQData* data = info->proc->GetIRQData();
    if (data == nullptr) {
        g_interruptMap.unlock();
        return false;
    }

    x86_64_ProcessorIRQData::HandlerData* handlerData = data->handlerMap.get(info->interrupt);
    if (handlerData == nullptr) {
        g_interruptMap.unlock();
        return false;
    }

    x86_64_IOAPIC* ioapic = x86_64_GetIOAPICForGSI(GSI);
    if (ioapic == nullptr) {
        g_interruptMap.unlock();
        return false; // Invalid GSI or I/O APICs aren't ready
    }

    // start by ensuring it is masked before we start deleting stuff
    uint64_t index = GSI - ioapic->GetGSIBase();
    x86_64_IOAPIC_RedirectionEntry entry = ioapic->GetRedirectionEntry(index);
    entry.Masked = 1;
    entry.Vector = 0;
    ioapic->SetRedirectionEntry(index, entry);

    g_interruptMap.remove(GSI);
    g_interruptMap.unlock();

    data->handlerMap.remove(info->interrupt);

    delete handlerData;
    delete info;

    return true;
}

bool x86_64_MaskGSI(uint32_t GSI) {
    x86_64_IOAPIC* ioapic = x86_64_GetIOAPICForGSI(GSI);
    if (ioapic == nullptr)
        return false; // Invalid GSI or I/O APICs aren't ready

    uint64_t index = GSI - ioapic->GetGSIBase();
    x86_64_IOAPIC_RedirectionEntry entry = ioapic->GetRedirectionEntry(index);
    entry.Masked = 1;
    ioapic->SetRedirectionEntry(index, entry);

    return true;
}

bool x86_64_UnmaskGSI(uint32_t GSI) {
    x86_64_IOAPIC* ioapic = x86_64_GetIOAPICForGSI(GSI);
    if (ioapic == nullptr)
        return false; // Invalid GSI or I/O APICs aren't ready

    uint64_t index = GSI - ioapic->GetGSIBase();
    x86_64_IOAPIC_RedirectionEntry entry = ioapic->GetRedirectionEntry(index);
    entry.Masked = 0;
    ioapic->SetRedirectionEntry(index, entry);

    return true;
}
