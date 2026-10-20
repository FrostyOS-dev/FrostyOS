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

#include "Time.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <util.h>

#include <HAL/Time.hpp>

#include <Scheduling/Event.hpp>
#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

int sys_clockget(int clock, timespec* time) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();

    timespec t;
    
    switch (clock) {
    case CLOCK_REALTIME:
        t.tv_sec = HAL_GetUnixEpochTime();
        if (t.tv_sec == INT64_MAX)
            return -ENOSYS;
        t.tv_nsec = 0;
        break;
    case CLOCK_MONOTONIC:
    case CLOCK_BOOTTIME: {
        uint64_t ns = HAL_GetNSTicks();
        t.tv_nsec = ns % 1'000'000'000;
        t.tv_sec = ns / 1'000'000'000;
        break;
    }
    default:
        return -ENOSYS;
    }

    if (!UserWrite(time, &t, sizeof(timespec), proc))
        return -EFAULT;

    return ESUCCESS;
}

int sys_sleep(timespec* time) {
    constexpr uint64_t NS = UINT64_C(1'000'000'000);
    Process* proc = Thread::GetCurrentThread()->GetParent();

    timespec req;
    if (!UserRead(time, &req, sizeof(timespec), proc))
        return -EFAULT;
    if (req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= (int64_t)NS)
        return -EINVAL;

    uint64_t reqNS;
    if (static_cast<uint64_t>(req.tv_sec) >= (UINT64_MAX / NS) / 2)
        reqNS = UINT64_MAX / 2; // clamp extremely long durations
    else
        reqNS = static_cast<uint64_t>(req.tv_sec) * NS + static_cast<uint64_t>(req.tv_nsec);
    if (reqNS == 0)
        return ESUCCESS;

    uint64_t deadline = HAL_GetNSTicks() + reqNS;
    while (true) {
        uint64_t now = HAL_GetNSTicks();
        if (now >= deadline)
            return ESUCCESS;

        uint64_t leftMS = DIV_ROUNDUP(deadline - now, 1'000'000);
        int rc = Event::WaitOnEvents(nullptr, 0, leftMS);

        if (rc == -EINTR) {
            now = HAL_GetNSTicks();
            if (now >= deadline)
                return ESUCCESS;
            uint64_t left = deadline - now;
            timespec rem = { static_cast<int64_t>(left / NS), static_cast<int64_t>(left % NS) };
            if (!UserWrite(time, &rem, sizeof(timespec), proc))
                return -EFAULT;
            return -EINTR;
        }
    }
}
