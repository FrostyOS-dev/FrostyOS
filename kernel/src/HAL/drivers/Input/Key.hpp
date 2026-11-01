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

#ifndef _HAL_INPUT_KEY_HPP
#define _HAL_INPUT_KEY_HPP

#include <frostyos/asm/key.h>

enum class Key {
#define ENUMERATE_KEY(c, s) c,
    __FOSI_ENUMERATE_INPUT_KEYS(ENUMERATE_KEY)
#undef ENUMERATE_KEY
};

inline constexpr const char* GetKeyName(Key k) {
    switch (k) {
#define ENUMERATE_KEY(c, s) case Key::c: return s;
        __FOSI_ENUMERATE_INPUT_KEYS(ENUMERATE_KEY)
#undef ENUMERATE_KEY
    default:
        return "Unknown";
    }
}

#endif /* _HAL_INPUT_KEY_HPP */