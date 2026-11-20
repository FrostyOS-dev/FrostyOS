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

#include <spinlock.h>
#include <stdint.h>
#include <time.h>
#include <util.h>

#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

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
        HAL_SleepNS(ms * 1'000'000);
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
#else
    return false;
#endif
}

int64_t HAL_ReadRTC() {
    TimeOfDay time = {};
    if (!HAL_GetTimeOfDay(&time))
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

// Wall clock
namespace {
    constexpr int64_t NS_PER_SEC = 1'000'000'000;
    constexpr int64_t NS_PER_MS = 1'000'000;

    int64_t g_realtimeOffsetNS = 0;
    bool g_realtimeValid = false;
    uint64_t g_realtimeGen = 0;
    spinlock_t g_realtimeLock = SPINLOCK_DEFAULT_VALUE;

    inline int64_t MonoNS() {
        return static_cast<int64_t>(HAL_GetNSTicks());
    }

    void PublishOffsetLocked(int64_t offset) {
        __atomic_store_n(&g_realtimeOffsetNS, offset, __ATOMIC_RELEASE);
        __atomic_store_n(&g_realtimeValid, true, __ATOMIC_RELEASE);
        __atomic_store_n(&g_realtimeGen, g_realtimeGen + 1, __ATOMIC_RELEASE);
    }

    uint64_t LoadGen() {
        return __atomic_load_n(&g_realtimeGen, __ATOMIC_ACQUIRE);
    }

    bool TryPublishOffset(int64_t offset, uint64_t expectedGen) {
        spinlock_acquire_int(&g_realtimeLock);
        bool ok = g_realtimeGen == expectedGen;
        if (ok)
            PublishOffsetLocked(offset);
        spinlock_release_int(&g_realtimeLock);
        return ok;
    }
}

int64_t HAL_GetRealtimeNS() {
    return MonoNS() + HAL_GetRealtimeOffsetNS();
}

int64_t HAL_GetRealtimeOffsetNS() {
    return __atomic_load_n(&g_realtimeOffsetNS, __ATOMIC_ACQUIRE);
}

void HAL_SetRealtimeNS(int64_t ns) {
    int64_t mono = MonoNS();
    spinlock_acquire_int(&g_realtimeLock);
    PublishOffsetLocked(ns - mono);
    spinlock_release_int(&g_realtimeLock);
}

bool HAL_RealtimeValid() {
    return __atomic_load_n(&g_realtimeValid, __ATOMIC_ACQUIRE);
}

timespec HAL_GetUnixEpochTime() {
    if (!HAL_RealtimeValid())
        return {0, 0};
    int64_t ns = HAL_GetRealtimeNS();
    return {ns / NS_PER_SEC, ns % NS_PER_SEC};
}

#ifdef __x86_64__

namespace {
    bool FindSecondEdgeCoarse(int64_t* edgeMono) {
        uint8_t last;
        if (!x86_64_CMOSReadSecondsStable(&last))
            return false;

        int64_t prev = MonoNS();
        int64_t deadline = prev + 1500 * NS_PER_MS;
        while (MonoNS() < deadline) {
            HAL_Sleep(1);
            uint8_t s;
            if (!x86_64_CMOSReadSecondsStable(&s))
                return false;
            int64_t now = MonoNS();
            if (s != last) {
                *edgeMono = prev + (now - prev) / 2;
                return true;
            }
            prev = now;
        }
        return false;
    }

    bool WaitForUpdateEnd(int64_t timeoutNS, int64_t* edgeMono) {
        int64_t deadline = MonoNS() + timeoutNS;
        bool sawUpdate = false;
        while (MonoNS() < deadline) {
            if (x86_64_CMOSUpdateInProgress())
                sawUpdate = true;
            else if (sawUpdate) {
                *edgeMono = MonoNS();
                return true;
            }
        }
        return false;
    }

    bool RefineAgainstRTC(uint64_t startGen, int64_t predictedEdge) {
        for (int attempt = 0; attempt < 3; attempt++) {
            int64_t now = MonoNS();
            int64_t wake = predictedEdge - 4 * NS_PER_MS;
            if (wake <= now) {
                int64_t offset = ((now - wake) / NS_PER_SEC + 1) * NS_PER_SEC;
                predictedEdge += offset;
                wake += offset;
            }
            HAL_Sleep((wake - now) / NS_PER_MS);

            int64_t before = HAL_ReadRTC();
            if (before == INT64_MAX)
                return false;

            int64_t edge;
            if (!WaitForUpdateEnd(20 * NS_PER_MS, &edge)) {
                predictedEdge += NS_PER_SEC;
                continue;
            }

            uint8_t sec;
            if (!x86_64_CMOSReadSecondsStable(&sec) || sec != static_cast<uint8_t>((before + 1) % 60)) {
                predictedEdge += NS_PER_SEC;
                continue;
            }

            int64_t newOffset = (before + 1) * NS_PER_SEC - edge;

            int64_t delta = newOffset - HAL_GetRealtimeOffsetNS();
            if (delta < 0)
                delta = -delta;
            if (delta > 2 * NS_PER_SEC)
                return false;

            return TryPublishOffset(newOffset, startGen);
        }
        return false;
    }

    void WallClockRefineMain(void* data) {
        uint64_t startGen = reinterpret_cast<uintptr_t>(data);
        int64_t edge;
        if (FindSecondEdgeCoarse(&edge))
            RefineAgainstRTC(startGen, edge + NS_PER_SEC);
        Thread::ExitCurrentKernelThread();
    }
}

#endif /* __x86_64__ */

void HAL_InitWallClock() {
    int64_t seconds = HAL_ReadRTC();
    if (seconds == INT64_MAX)
        return;
    int64_t mono = MonoNS();

    spinlock_acquire_int(&g_realtimeLock);
    if (!g_realtimeValid)
        PublishOffsetLocked(seconds * NS_PER_SEC - mono);
    uint64_t gen = g_realtimeGen;
    spinlock_release_int(&g_realtimeLock);

#ifdef __x86_64__
    Thread::CreateKernelThread({&WallClockRefineMain, reinterpret_cast<void*>(gen)});
#else
    (void)gen;
#endif
}
