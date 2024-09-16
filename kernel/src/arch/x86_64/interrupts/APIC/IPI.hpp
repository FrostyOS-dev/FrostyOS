/*
Copyright (©) 2024  Frosty515

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

#ifndef _x86_64_APIC_IPI_HPP
#define _x86_64_APIC_IPI_HPP

#include <stdint.h>

#include "LAPIC.hpp"

enum class x86_64_IPI_Delivery_Mode {
    FIXED = 0b000,
    LOWEST_PRIORITY = 0b001,
    SMI = 0b010,
    NMI = 0b100,
    INIT = 0b101,
    STARTUP = 0b110
};

enum class x86_64_IPI_Destination_Mode {
    PHYSICAL = 0b0,
    LOGICAL = 0b1
};

enum class x86_64_IPI_Destination_Shorthand {
    NONE = 0b00,
    SELF = 0b01,
    ALL_INCLUDING_SELF = 0b10,
    ALL_EXCLUDING_SELF = 0b11
};

enum class x86_64_IPI_Level {
    DEASSERT = 0b0,
    ASSERT = 0b1
};

enum class x86_64_IPI_Trigger_Mode {
    EDGE = 0b0,
    LEVEL = 0b1
};

void x86_64_Request_IPI(x86_64_LAPIC* LAPIC, uint8_t vector, x86_64_IPI_Delivery_Mode delivery_mode, x86_64_IPI_Destination_Mode destination_mode, x86_64_IPI_Destination_Shorthand destination_shorthand, x86_64_IPI_Level level, x86_64_IPI_Trigger_Mode trigger_mode, uint8_t destination);


#endif /* _x86_64_APIC_IPI_HPP */