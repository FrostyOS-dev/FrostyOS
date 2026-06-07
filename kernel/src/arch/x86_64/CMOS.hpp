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

#ifndef _x86_64_CMOS_HPP
#define _x86_64_CMOS_HPP

#include <stdint.h>

#include <HAL/Time.hpp>

enum class x86_64_CMOSRegister : uint8_t {
    Seconds    = 0x00, // 0-59
    Minutes    = 0x02, // 0-59
    Hours      = 0x04, // 0-23 in 24 hour mode, 1-12 in 12 hour mode, highest bit set if pm
    Weekday    = 0x06, // 1-7, Sunday = 1
    DayOfMonth = 0x07, // 1-31
    Month      = 0x08, // 1-12
    Year       = 0x09, // 0-99
    Century    = 0xFF, // 19-20, register number is retrieved later
    StatusA    = 0x0A,
    StatusB    = 0x0B
};

#define CMOS_SELECT 0x70
#define CMOS_DATA 0x71

void x86_64_InitCMOS(uint8_t centuryReg);
uint8_t x86_64_ReadCMOS(x86_64_CMOSRegister reg);
void x86_64_WriteCMOS(x86_64_CMOSRegister reg, uint8_t value);

bool x86_64_CMOSGetTimeOfDay(TimeOfDay* out);

#endif /* _x86_64_CMOS_HPP */