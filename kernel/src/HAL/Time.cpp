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

#include "Processor.hpp"
#include "Time.hpp"

#include "drivers/HPET.hpp"

#include <stdint.h>
#include <util.h>

#include <Scheduling/Scheduler.hpp>

#ifdef __x86_64__
#include <arch/x86_64/CMOS.hpp>
#include <arch/x86_64/Processor.hpp>

#include <arch/x86_64/interrupts/APIC/LocalAPIC.hpp>
#endif

uint64_t g_HALTimerTicks = 0; // in ms
bool g_HALTimeReady = false;

void HAL_InitTime() {
    g_BSP->InitTime();
    g_HALTimeReady = true;
}

void HAL_TimerTick(Processor* proc, uint64_t ticks, void *data) {
    if (proc->isBSP())
        g_HALTimerTicks += ticks;
    Scheduler_PrepForTimerTick(ticks, data);
}

void HAL_EndTimerTick() {
#ifdef __x86_64__

    x86_64_Processor* proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    if (proc == nullptr)
        return;

    x86_64_LAPIC* lapic = proc->GetLAPIC();
    if (lapic != nullptr)
        lapic->SendEOI();

#endif
}

uint64_t HAL_GetTicks() {
    return g_HALTimerTicks;
}

uint64_t HAL_GetNSTicks() {
    if (g_HPET != nullptr)
        return g_HPET->GetNSTicks();
    return g_HALTimerTicks * 1'000'000;
}

void HAL_Sleep(uint64_t ms) {
    if (Scheduler::isRunning())
        Scheduler::SleepCurrentThread(ms);
    else
        HAL_SleepNS(ms * 1'000'00);
}

void HAL_SleepNS(uint64_t ns) {
    if (!g_HALTimeReady)
        return;
    if (g_HPET != nullptr) {
        uint64_t end = g_HPET->GetNSTicks() + ns;
        while (g_HPET->GetNSTicks() < end)
            PAUSE();
    } else {
        uint64_t end = g_HALTimerTicks + DIV_ROUNDUP(ns, 1'000'000);
        while (g_HALTimerTicks < end)
            PAUSE();
    }
}

bool HAL_GetTimeOfDay(TimeOfDay* time) {
#ifdef __x86_64__
    return x86_64_CMOSGetTimeOfDay(time);
#endif
}

int64_t HAL_GetUnixEpochTime() {
    TimeOfDay time = {};
    bool rc = HAL_GetTimeOfDay(&time);
    if (!rc)
        return INT64_MAX;
    int64_t days = DaysSinceUnixEpoch(time.year, time.month, time.dayOfMonth);
    return days * 86400 + time.hours * 3600 + time.minutes * 60 + time.seconds;
}

int64_t DaysSinceUnixEpoch(int32_t year, uint8_t month, uint8_t day) {
    year -= month <= 2 ? 1 : 0;
    int32_t era = (year >= 0 ? year : year - 399) / 400;
    uint32_t yoe = (uint32_t)(year - era * 400);
    uint32_t doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + (int32_t)doe) - 719468;
}
