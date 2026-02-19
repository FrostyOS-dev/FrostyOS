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

#ifndef _DRIVERS_HPET_HPP
#define _DRIVERS_HPET_HPP

#include <stdint.h>

enum class HPET_Register {
    CAP_ID = 0,
    CONFIG = 0x10,
    INT_STATUS = 0x20,
    MAIN_COUNTER = 0xF0,
    TIMER0_CONFIG = 0x100,
    TIMER0_COMP = 0x108,
    TIMER0_FSB_INT = 0x110,
};

struct [[gnu::packed]] HPET_GeneralCapID {
    uint8_t REV_ID;
    uint8_t NUM_TIM_CAP : 5;
    uint8_t COUNT_SIZE_CAP : 1;
    uint8_t Reserved : 1;
    uint8_t LEG_RT_CAP : 1;
    uint16_t VENDOR_ID;
    uint32_t COUNTER_CLK_PERIOD;
};

class HPET {
public:
    HPET(uint64_t address);
    ~HPET();

    bool Init();

    uint64_t GetNSTicks(); // Get ticks in nanoseconds

private:
    uint64_t m_address;
    uint32_t m_period;
    uint32_t m_nsperiod;
    uint64_t m_freq;
};

extern HPET* g_HPET;

#endif /* _DRIVERS_HPET_HPP */