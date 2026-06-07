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

#include "CMOS.hpp"
#include "IO.h"

#include <HAL/Processor.hpp>

bool g_hasCMOS = false;
uint8_t CMOS_CENTURY_REG = 0;
uint8_t g_CMOSFlags = 0;

void x86_64_InitCMOS(uint8_t centuryReg) {
    CMOS_CENTURY_REG = centuryReg;
    g_CMOSFlags = x86_64_ReadCMOS(x86_64_CMOSRegister::StatusB);
    g_hasCMOS = true;
}

uint8_t x86_64_ReadCMOS(x86_64_CMOSRegister reg) {
    if (reg == x86_64_CMOSRegister::Century)
        reg = (x86_64_CMOSRegister)CMOS_CENTURY_REG;
    int intState = Processor::DisableInterrupts();
    x86_64_outb(CMOS_SELECT, (uint8_t)reg);
    uint8_t ret = x86_64_inb(CMOS_DATA);
    Processor::EnableInterrupts(intState);
    return ret;
}

void x86_64_WriteCMOS(x86_64_CMOSRegister reg, uint8_t value) {
    if (reg == x86_64_CMOSRegister::Century)
        reg = (x86_64_CMOSRegister)CMOS_CENTURY_REG;
    int intState = Processor::DisableInterrupts();
    x86_64_outb(CMOS_SELECT, (uint8_t)reg);
    x86_64_outb(CMOS_DATA, value);
    Processor::EnableInterrupts(intState);
}

uint8_t x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister reg) {
    uint8_t value = x86_64_ReadCMOS(reg);
    if (g_CMOSFlags & 4)
        return value;
    return (value & 0xf) + (value >> 4) * 10;
}

bool x86_64_CMOSGetTimeOfDay(TimeOfDay* out) {
    if (!g_hasCMOS || out == nullptr)
        return false;
    uint8_t val;
    do {
        val = x86_64_ReadCMOS(x86_64_CMOSRegister::StatusA);
    } while ((val & (1 << 7)) > 0);
    out->seconds = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::Seconds);
    out->minutes = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::Minutes);
    uint8_t hours = x86_64_ReadCMOS(x86_64_CMOSRegister::Hours);
    out->dayOfMonth = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::DayOfMonth);
    out->month = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::Month);
    out->year = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::Year);
    uint16_t century = CMOS_CENTURY_REG != 0 ? x86_64_ReadCMOSMaybeBCD((x86_64_CMOSRegister)CMOS_CENTURY_REG) : 20;
    out->year += century * 100;
    if ((g_CMOSFlags & 2) == 0) { // 12 hour
        bool pm = hours & 0x80;
        hours &= 0x7F;
        if ((g_CMOSFlags & 4) == 0)
            hours = (hours & 0xf) + (hours >> 4) * 10;
        hours *= pm ? 2 : 1;
        out->hours = hours == 24 ? 0 : hours;
    } else if ((g_CMOSFlags & 4) == 0)
        out->hours = (hours & 0xf) + (hours >> 4) * 10;
    else
        out->hours = hours;
    return true;
}
