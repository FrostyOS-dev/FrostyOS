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

#ifndef _TTY_KBDTRANSLATOR_HPP
#define _TTY_KBDTRANSLATOR_HPP

#include <stddef.h>
#include <stdint.h>

#include <HAL/drivers/Input/InputEvent.hpp>
#include <HAL/drivers/Input/Key.hpp>

#define KT_MAX_SEQ 16

class KBDTranslator {
public:
    size_t HandleKey(Key key, bool released, char* out);
    void SetApplicationCursorKeys(bool on);

    bool Shift() const;
    bool Ctrl() const;
    bool Alt() const;

private:
    enum : uint8_t {
        M_LSHIFT =  1,
        M_RSHIFT =  2,
        M_LCTRL  =  4,
        M_RCTRL  =  8,
        M_LALT   = 16,
        M_RALT   = 32,
    };

    bool KeyToChar(Key k, char& out) const;

    uint8_t m_held = 0;
    bool m_caps = false;
    bool m_numLock = true;
    bool m_appCursor = false;
};

#endif /* _TTY_KBDTRANSLATOR_HPP */