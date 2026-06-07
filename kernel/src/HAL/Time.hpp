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

#ifndef _HAL_TIME_HPP
#define _HAL_TIME_HPP

#include <stdint.h>

#include "Processor.hpp"

struct TimeOfDay {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours; // always 24-hour
    uint8_t dayOfMonth;
    uint8_t month;
    uint32_t year;
};

void HAL_InitTime();
void HAL_TimerTick(Processor* proc, uint64_t ticks, void* data);
void HAL_EndTimerTick();

uint64_t HAL_GetTicks();
uint64_t HAL_GetNSTicks();

void HAL_Sleep(uint64_t ms);
void HAL_SleepNS(uint64_t ns);

bool HAL_GetTimeOfDay(TimeOfDay* time);
int64_t HAL_GetUnixEpochTime(); // returns INT64_MAX on error

int64_t DaysSinceUnixEpoch(int32_t year, uint8_t month, uint8_t day);

#endif /* _HAL_TIME_HPP */