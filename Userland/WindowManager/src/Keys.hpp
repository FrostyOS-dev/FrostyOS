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

#ifndef _WM_KEY_HPP
#define _WM_KEY_HPP

#include <stdint.h>
#include <string>

#include <asm/input.h>

namespace WM {
    typedef uint32_t KeyCode;
    constexpr uint32_t KeyCount = FOSI_KEY_COUNT;

    namespace Mod {
        enum : uint32_t {
            Shift = 1,
            Ctrl = 2,
            Alt = 4,
            Super = 8,
            CapsLock = 16,
            NumLock = 32
        };

        constexpr uint32_t Shortcut = Shift | Ctrl | Alt | Super;
    }

    bool IsMouseButton(KeyCode k);
    uint32_t ModifierBit(KeyCode k);
    bool KeyFromName(const std::string& name, KeyCode& out);
    const char* KeyIdentifier(KeyCode k);
}

#endif /* _WM_KEY_HPP */