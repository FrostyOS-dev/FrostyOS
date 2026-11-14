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

#ifndef _FOS_LIBWM_MESSAGE_HPP
#define _FOS_LIBWM_MESSAGE_HPP

#include <stddef.h>
#include <stdint.h>

namespace LibWM {
    struct MessageHeader {
        uint32_t object;
        uint16_t opcode;
        uint16_t size;
    };
    static_assert(sizeof(MessageHeader) == 8);

    constexpr uint32_t DisplayObject = 0;
    constexpr size_t MaxMessageSize = 4096;
    constexpr size_t MaxFdsPerMessage = 4;
    constexpr size_t MaxOutputBacklog = 1048576; // 1024 * 1024
    constexpr size_t MaxPendingInputFds = 64;
}

#endif /* _FOS_LIBWM_MESSAGE_HPP */