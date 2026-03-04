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

#include "../Memory/PagingUtil.hpp"

#include <spinlock.h>
#include <string.h>
#include <util.h>

#include <Scheduling/Scheduler.hpp>

namespace x86_64_GlobalNMI {
    struct GlobalNMIData {
        x86_64_NMIType type;
        x86_64_NMI_InvPagesData invPagesData;
        uint64_t cpuCount;
        uint64_t maxCPUCount;
        spinlock_t lock;
    };

    GlobalNMIData g_NMIData = {x86_64_NMIType::NONE, {0, 0}, 0, 0, SPINLOCK_DEFAULT_VALUE};

    void SetData(x86_64_NMIType type, void* data, uint64_t cpuCount) {
        spinlock_acquire(&g_NMIData.lock);
        g_NMIData.type = type;
        switch (type) {
        case x86_64_NMIType::INVPAGES:
            memcpy(&g_NMIData.invPagesData, data, sizeof(x86_64_NMI_InvPagesData));
            break;
        default:
            break;
        }
        if (cpuCount == 0)
            cpuCount = Scheduler::GetProcessorCount();
        __atomic_store_n(&g_NMIData.cpuCount, cpuCount, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_NMIData.maxCPUCount, cpuCount, __ATOMIC_SEQ_CST);
        spinlock_release(&g_NMIData.lock);
    }

    void WaitForCPUs(uint64_t count) {
        if (count == 0)
            count = __atomic_load_n(&g_NMIData.maxCPUCount, __ATOMIC_SEQ_CST);

        uint64_t flags = 0;
        __asm__ volatile ("pushfq;pop %0" : "=r"(flags));
        flags &= 1 << 9; // IF

        x86_64_DisableInterrupts();

        while ((int64_t)(__atomic_load_n(&g_NMIData.cpuCount, __ATOMIC_SEQ_CST) - count) > 0)
            __asm__ volatile ("pause" ::: "memory");

        if (flags > 0)
            x86_64_EnableInterrupts();
    }

    void Raise(x86_64_LAPIC* lapic, x86_64_NMIType type, void* data, uint64_t cpuCount, bool wait) {
        if (cpuCount == 0)
            cpuCount = Scheduler::GetProcessorCount();
        SetData(type, data, cpuCount);
        x86_64_IPI::RaiseIPI(lapic, 0, 0, x86_64_IPI::DeliveryMode::NMI, x86_64_IPI::TriggerMode::Edge, x86_64_IPI::DestMode::Physical, x86_64_IPI::DestShort::AllExcSelf);
        if (wait)
            WaitForCPUs(cpuCount);
    }

}

bool x86_64_HandleNMI(x86_64_ISR_Frame* frame) {
    using namespace x86_64_GlobalNMI;
    spinlock_acquire(&g_NMIData.lock);
    bool status = false;
    if (g_NMIData.type != x86_64_NMIType::NONE) {
        status = true;
        x86_64_NMIType type = g_NMIData.type;
        uint64_t count = __atomic_sub_fetch(&g_NMIData.cpuCount, 1, __ATOMIC_SEQ_CST);
        if (count == 0) {
            g_NMIData.type = x86_64_NMIType::NONE;
            g_NMIData.maxCPUCount = 0;
        }
        switch (type) {
        case x86_64_NMIType::HALT:
            x86_64_DisableInterrupts();
            spinlock_release(&g_NMIData.lock);
            while (true)
                __asm__ volatile ("hlt");
        case x86_64_NMIType::INVPAGES:
            x86_64_InvalidatePages(g_NMIData.invPagesData.start, g_NMIData.invPagesData.pageCount * PAGE_SIZE);
            break;
        }
    }
    spinlock_release(&g_NMIData.lock);
    return status;
}
