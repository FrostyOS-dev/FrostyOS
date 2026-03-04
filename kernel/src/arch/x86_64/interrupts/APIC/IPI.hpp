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

#ifndef _x86_64_IPI_HPP
#define _x86_64_IPI_HPP

#include <stdint.h>

class x86_64_LAPIC;

namespace x86_64_IPI {
    enum class DestShort : uint8_t {
        None = 0,
        Self = 1,
        AllIncSelf = 2,
        AllExcSelf = 3,
    };

    enum class TriggerMode : uint8_t {
        Edge = 0,
        Level = 1
    };

    enum class Level : uint8_t {
        Deassert = 0,
        Assert = 1
    };

    enum class DestMode : uint8_t {
        Physical = 0,
        Logical = 1
    };

    enum class DeliveryMode : uint8_t {
        Fixed = 0,
        Lowest = 1,
        SMI = 2,
        NMI = 4,
        INIT = 5,
        Startup = 6
    };

    void RaiseIPI(x86_64_LAPIC* lapic, uint8_t dest, uint8_t vector, DeliveryMode deliveryMode = DeliveryMode::Fixed, TriggerMode mode = TriggerMode::Edge, DestMode destMode = DestMode::Physical, DestShort shorthand = DestShort::None, Level level = Level::Assert);
}

#endif /* _x86_64_IPI_HPP */