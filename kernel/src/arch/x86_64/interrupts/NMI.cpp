/*
Copyright (©) 2026  Frosty515

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

#include "NMI.hpp"

#include "APIC/IPI.hpp"

#include "../ArchDefs.h"
#include "../Processor.hpp"

#include "../Memory/PagingUtil.hpp"

#include "../Scheduling/TaskUtil.hpp"

#include "debug.h"

#include <spinlock.h>
#include <string.h>
#include <util.h>

#include <HAL/Processor.hpp>

#include <Scheduling/Scheduler.hpp>

namespace x86_64_GlobalNMI {
    struct GlobalNMIData {
        x86_64_NMIType type;
        x86_64_NMI_InvPagesData invPagesData;
        bool yieldData;
        uint64_t cpuCount;
        uint64_t maxCPUCount;
        spinlock_t lock;
        spinlock_t raiseLock;
    };

    GlobalNMIData g_NMIData = {x86_64_NMIType::NONE, {0, 0}, false, 0, 0, SPINLOCK_DEFAULT_VALUE, SPINLOCK_DEFAULT_VALUE};

    void ForceAllowRaise() {
        spinlock_release(&g_NMIData.raiseLock);
    }

    void SetData(x86_64_NMIType type, void* data, uint64_t cpuCount) {
        spinlock_acquire(&g_NMIData.lock);
        g_NMIData.type = type;
        switch (type) {
        case x86_64_NMIType::INVPAGES:
            memcpy(&g_NMIData.invPagesData, data, sizeof(x86_64_NMI_InvPagesData));
            break;
        case x86_64_NMIType::YIELD:
            assert(false);
            g_NMIData.yieldData = *static_cast<bool*>(data);
            break;
        default:
            break;
        }
        if (cpuCount == 0)
            cpuCount = Scheduler::GetProcessorCount() - 1; // Don't raise on current cpu
        __atomic_store_n(&g_NMIData.cpuCount, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_NMIData.maxCPUCount, cpuCount, __ATOMIC_SEQ_CST);
        spinlock_release(&g_NMIData.lock);
    }

    void WaitForCPUs(uint64_t count) {
        if (count == 0)
            count = __atomic_load_n(&g_NMIData.maxCPUCount, __ATOMIC_SEQ_CST);

        int flags = Processor::DisableInterrupts();

        while (__atomic_load_n(&g_NMIData.cpuCount, __ATOMIC_SEQ_CST) < count)
            __asm__ volatile ("pause" ::: "memory");

        Processor::EnableInterrupts(flags);
    }

    void Raise(x86_64_LAPIC* lapic, x86_64_NMIType type, void* data, uint64_t cpuCount, bool wait) {
        if (cpuCount == 0)
            cpuCount = Scheduler::GetProcessorCount() - 1; // Don't raise on current cpu
        spinlock_acquire(&g_NMIData.raiseLock);
        SetData(type, data, cpuCount);
        x86_64_IPI::RaiseIPI(lapic, 0, 0, x86_64_IPI::DeliveryMode::NMI, x86_64_IPI::TriggerMode::Edge, x86_64_IPI::DestMode::Physical, x86_64_IPI::DestShort::AllExcSelf);
        if (wait)
            WaitForCPUs(cpuCount);
        spinlock_release(&g_NMIData.raiseLock);
    }

}

namespace x86_64_LocalNMI { // for NMIs on a specific CPU
    struct LocalNMIData {
        x86_64_NMIType type;
        x86_64_NMI_InvPagesData invPagesData;
        bool yieldData;
        spinlock_t lock;
        uint64_t handled;
    };

    void Init() {
        x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
        if (proc == nullptr)
            return;

        proc->NMIData = new LocalNMIData {x86_64_NMIType::NONE, {0, 0}, false, SPINLOCK_DEFAULT_VALUE, 0};
    }

    void Raise(x86_64_Processor* current, Scheduler::ProcessorState* target, x86_64_NMIType type, void* data, bool wait) {
        if (target == nullptr || target->processor == nullptr)
            return;
        return Raise(current, static_cast<x86_64_Processor*>(target->processor), type, data, wait);
    }

    void Raise(x86_64_Processor* current, x86_64_Processor* target, x86_64_NMIType type, void* data, bool wait) {
        if (current == nullptr || target == nullptr)
            return;

        spinlock_acquire(&x86_64_GlobalNMI::g_NMIData.raiseLock);

        x86_64_LAPIC* currentLAPIC = current->GetLAPIC();
        if (currentLAPIC == nullptr) {
            spinlock_release(&x86_64_GlobalNMI::g_NMIData.raiseLock);
            return;
        }

        if (target == nullptr) {
            spinlock_release(&x86_64_GlobalNMI::g_NMIData.raiseLock);
            return;
        }

        x86_64_LAPIC* targetLAPIC = target->GetLAPIC();
        if (targetLAPIC == nullptr) {
            spinlock_release(&x86_64_GlobalNMI::g_NMIData.raiseLock);
            return; 
        }

        LocalNMIData* NMIData = static_cast<LocalNMIData*>(target->NMIData);
        if (NMIData == nullptr) {
            spinlock_release(&x86_64_GlobalNMI::g_NMIData.raiseLock);
            return; 
        }

        spinlock_acquire(&NMIData->lock);
        NMIData->type = type;
        switch (type) {
        case x86_64_NMIType::INVPAGES:
            memcpy(&NMIData->invPagesData, data, sizeof(x86_64_NMI_InvPagesData));
            break;
        case x86_64_NMIType::YIELD:
            assert(false);
            NMIData->yieldData = *static_cast<bool*>(data);
            break;
        default:
            break;
        }
        __atomic_store_n(&NMIData->handled, 0, __ATOMIC_SEQ_CST);
        spinlock_release(&NMIData->lock);

        x86_64_IPI::RaiseIPI(currentLAPIC, targetLAPIC->GetID(), 0, x86_64_IPI::DeliveryMode::NMI, x86_64_IPI::TriggerMode::Edge, x86_64_IPI::DestMode::Physical, x86_64_IPI::DestShort::None);
        
        if (wait) {
            int flags = Processor::DisableInterrupts();

            while (__atomic_load_n(&NMIData->handled, __ATOMIC_SEQ_CST) == 0)
                __asm__ volatile ("pause" ::: "memory");

            Processor::EnableInterrupts(flags);
        }

        spinlock_release(&x86_64_GlobalNMI::g_NMIData.raiseLock);
    }
}

bool x86_64_HandleNMI(x86_64_ISR_Frame* frame) {
    debug_putc('N');
    using namespace x86_64_GlobalNMI;
    spinlock_acquire(&g_NMIData.lock);
    bool status = false;
    if (g_NMIData.type != x86_64_NMIType::NONE) {
        status = true;
        x86_64_NMIType type = g_NMIData.type;
        switch (type) {
        case x86_64_NMIType::HALT: {
            if (!Scheduler::isRunning() || !Scheduler::SaveOnInt(frame)) {
                x86_64_DisableInterrupts();
                uint64_t count = __atomic_add_fetch(&g_NMIData.cpuCount, 1, __ATOMIC_SEQ_CST);
                if (count == __atomic_load_n(&g_NMIData.maxCPUCount, __ATOMIC_SEQ_CST)) {
                    g_NMIData.type = x86_64_NMIType::NONE;
                    g_NMIData.maxCPUCount = 0;
                }
                spinlock_release(&g_NMIData.lock);
                dbgputc('H');
                while (true)
                    __asm__ volatile ("hlt");
            }
            dbgputc('h');
            x86_64_CreateHaltISRFrame(frame);
            break;
        }
        case x86_64_NMIType::INVPAGES:
            x86_64_InvalidatePages(g_NMIData.invPagesData.start, g_NMIData.invPagesData.pageCount * PAGE_SIZE);
            break;
        case x86_64_NMIType::YIELD: {
            assert(false);
            Scheduler::Yield(g_NMIData.yieldData, frame);
            break;
        }
        }
        uint64_t count = __atomic_add_fetch(&g_NMIData.cpuCount, 1, __ATOMIC_SEQ_CST);
        if (count == __atomic_load_n(&g_NMIData.maxCPUCount, __ATOMIC_SEQ_CST)) {
            g_NMIData.type = x86_64_NMIType::NONE;
            g_NMIData.maxCPUCount = 0;
        }
        spinlock_release(&g_NMIData.lock);
    } else {
        spinlock_release(&g_NMIData.lock);
        x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
        if (proc != nullptr && proc->NMIData != nullptr) {
            using namespace x86_64_LocalNMI;
            LocalNMIData* data = static_cast<LocalNMIData*>(proc->NMIData);
            spinlock_acquire(&data->lock);
            if (data->type != x86_64_NMIType::NONE) {
                status = true;
                switch (data->type) {
                case x86_64_NMIType::HALT:
                    if (!Scheduler::isRunning() || !Scheduler::SaveOnInt(frame)) {
                        x86_64_DisableInterrupts();
                        __atomic_store_n(&data->handled, 1, __ATOMIC_SEQ_CST);
                        spinlock_release(&data->lock);
                        while (true)
                            __asm__ volatile ("hlt");
                    }
                    x86_64_CreateHaltISRFrame(frame);
                    break;
                case x86_64_NMIType::INVPAGES:
                    x86_64_InvalidatePages(data->invPagesData.start, data->invPagesData.pageCount * PAGE_SIZE);
                    break;
                case x86_64_NMIType::YIELD:
                    Scheduler::Yield(data->yieldData, frame);
                    break;
                }
                __atomic_store_n(&data->handled, 1, __ATOMIC_SEQ_CST);
            }
            spinlock_release(&data->lock);
        }
    }
    debug_putc('n');
    return status;
}
