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
#include <HAL/Time.hpp>

bool g_hasCMOS = false;
uint8_t CMOS_CENTURY_REG = 0;
uint8_t g_CMOSFlags = 0;

#define CMOS_STATUSB_24H 2
#define CMOS_STATUSB_BINARY 4
#define CMOS_UIP (1 << 7)

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

static uint8_t FromBCD(uint8_t v) {
    return (v & 0xF) + (v >> 4) * 10;
}

static uint8_t ConvertValue(uint8_t v) {
    return (g_CMOSFlags & CMOS_STATUSB_BINARY) ? v : FromBCD(v);
}

uint8_t x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister reg) {
    return ConvertValue(x86_64_ReadCMOS(reg));
}

bool x86_64_CMOSUpdateInProgess() {
    return (x86_64_ReadCMOS(x86_64_CMOSRegister::StatusA) & CMOS_UIP) != 0;
}

static bool WaitForUIPClear() {
    for (size_t i = 0; i < 100000; i++) {
        if (!x86_64_CMOSUpdateInProgess())
            return true;
    }
    return false;
}

bool x86_64_CMOSReadSecondsStable(uint8_t* out) {
    if (!g_hasCMOS || out == nullptr)
        return false;
    if (!WaitForUIPClear())
        return false;
    *out = x86_64_ReadCMOSMaybeBCD(x86_64_CMOSRegister::Seconds);
    return true;
}

namespace {
    struct RawTime {
        uint8_t seconds;
        uint8_t minutes;
        uint8_t hours;
        uint8_t dayOfMonth;
        uint8_t month;
        uint8_t year;
        uint8_t century;
    };
}

static void ReadRawTime(RawTime* t) {
    t->seconds = x86_64_ReadCMOS(x86_64_CMOSRegister::Seconds);
    t->minutes = x86_64_ReadCMOS(x86_64_CMOSRegister::Minutes);
    t->hours = x86_64_ReadCMOS(x86_64_CMOSRegister::Hours);
    t->dayOfMonth = x86_64_ReadCMOS(x86_64_CMOSRegister::DayOfMonth);
    t->month = x86_64_ReadCMOS(x86_64_CMOSRegister::Month);
    t->year = x86_64_ReadCMOS(x86_64_CMOSRegister::Year);
    t->century = CMOS_CENTURY_REG != 0 ? x86_64_ReadCMOS(x86_64_CMOSRegister::Century) : 0;
}

static void DecodeRawTime(const RawTime& r, TimeOfDay* out) {
    out->seconds = ConvertValue(r.seconds);
    out->minutes = ConvertValue(r.minutes);
    out->dayOfMonth = ConvertValue(r.dayOfMonth);
    out->month = ConvertValue(r.month);

    uint8_t hours = r.hours;
    bool pm = false;
    bool is24h = (g_CMOSFlags & CMOS_STATUSB_24H) != 0;
    if (!is24h) {
        pm = (hours & 0x80) != 0;
        hours &= 0x7F;
    }
    hours = ConvertValue(hours);
    if (!is24h)
        hours = (hours % 12) + (pm ? 12 : 0);

    out->hours = hours;

    uint32_t century = CMOS_CENTURY_REG != 0 ? ConvertValue(r.century) : 20;
    if (century < 19 || century > 21) // protect against garbage left by firmware
        century = 20;
    out->year = ConvertValue(r.year) + century * 100;
}

bool x86_64_CMOSGetTimeOfDay(TimeOfDay* out) {
    if (!g_hasCMOS || out == nullptr)
        return false;
    
    RawTime prev, cur;
    if (!WaitForUIPClear())
        return false;
    ReadRawTime(&prev);

    for (int i = 0; i < 5; i++) {
        if (!WaitForUIPClear())
            return false;
        ReadRawTime(&cur);
        if (memcmp(&prev, &cur, sizeof(RawTime)) == 0) {
            DecodeRawTime(cur, out);
            return true;
        }
        prev = cur;
    }
    return false;
}
