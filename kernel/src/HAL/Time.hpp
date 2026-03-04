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

void HAL_InitTime();
void HAL_TimerTick(Processor* proc, uint64_t ticks, void* data);

uint64_t HAL_GetTicks();
uint64_t HAL_GetNSTicks();

void HAL_SleepNS(uint64_t ns);

#endif /* _HAL_TIME_HPP */