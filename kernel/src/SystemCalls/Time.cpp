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

#include <stdint.h>
#include <errno.h>
#include <time.h>

#include <HAL/Time.hpp>

#include <Scheduling/Process.hpp>
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
        t.tv_nsec = t.tv_sec * 1'000'000'000;
        break;
    case CLOCK_MONOTONIC:
    case CLOCK_BOOTTIME:
        t.tv_nsec = HAL_GetNSTicks();
        t.tv_sec = t.tv_nsec / 1'000'000'000;
        break;
    default:
        return -ENOSYS;
    }

    if (!UserWrite(time, &t, sizeof(timespec), proc))
        return -EFAULT;

    return ESUCCESS;
}
