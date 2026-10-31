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

#include "KBDTranslator.hpp"

#include <string.h>

#include <HAL/drivers/Input/Key.hpp>

enum class Form : uint8_t {
    Cursor,
    FnSS3,
    Tilde
};

struct SpecialKey {
    Key key;
    Form form;
    char code;
};

constexpr SpecialKey kSpecialKeys[] = {
    {Key::UP_ARROW, Form::Cursor, 'A'}, {Key::DOWN_ARROW, Form::Cursor, 'B'},
    {Key::LEFT_ARROW, Form::Cursor, 'D'}, {Key::RIGHT_ARROW, Form::Cursor, 'C'},
    {Key::HOME, Form::Cursor, 'H'}, {Key::END, Form::Cursor, 'F'},
    {Key::F1, Form::FnSS3, 'P'}, {Key::F2, Form::FnSS3, 'Q'},
    {Key::F3, Form::FnSS3, 'R'}, {Key::F4, Form::FnSS3, 'S'},
    {Key::INSERT, Form::Tilde, 2}, {Key::DELETE, Form::Tilde, 3},
    {Key::PAGE_UP, Form::Tilde, 5}, {Key::PAGE_DOWN, Form::Tilde, 6},
    {Key::F5, Form::Tilde, 15}, {Key::F6, Form::Tilde, 17}, {Key::F7, Form::Tilde, 18},
    {Key::F8, Form::Tilde, 19}, {Key::F9, Form::Tilde, 20}, {Key::F10, Form::Tilde, 21},
    {Key::F11, Form::Tilde, 23}, {Key::F12, Form::Tilde, 24}
};

uint8_t ModifierBit(Key k) {
    switch (k) {
    case Key::LEFT_SHIFT:
        return 1;
    case Key::RIGHT_SHIFT:
        return 2;
    case Key::LEFT_CTRL:
        return 4;
    case Key::RIGHT_CTRL:
        return 8;
    case Key::LEFT_ALT:
        return 16;
    case Key::RIGHT_ALT:
        return 32;
    }
    return 0;
}

Key NumpadToNav(Key k) {
    switch (k) {
    case Key::NP_SEVEN:
        return Key::HOME;
    case Key::NP_EIGHT:
        return Key::UP_ARROW;
    case Key::NP_NINE:
        return Key::PAGE_UP;
    case Key::NP_FOUR:
        return Key::LEFT_ARROW;
    case Key::NP_FIVE:
        return Key::UNKNOWN;
    case Key::NP_SIX:
        return Key::RIGHT_ARROW;
    case Key::NP_ONE:
        return Key::END;
    case Key::NP_TWO:
        return Key::DOWN_ARROW;
    case Key::NP_THREE:
        return Key::PAGE_DOWN;
    case Key::NP_ZERO:
        return Key::INSERT;
    case Key::NP_DOT:
        return Key::DELETE;
    }
    return k;
}

char ShiftSymbol(char c) {
    static const char from[] = "`1234567890-=[]\\;',./";
    static const char to[] = "~!@#$%^&*()_+{}|:\"<>?";
    for (size_t i = 0; from[i]; i++)
        if (from[i] == c)
            return to[i];
    return c;
}

char ApplyCtrl(char c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 1;
    if (c >= '@' && c <= '_')
        return c & 0x1F;
    if (c == ' ')
        return 0;
    if (c == '?')
        return 0x7F;
    return c;
}

size_t KBDTranslator::HandleKey(Key key, bool released, char* out) {
    if (uint8_t bit = ModifierBit(key); bit != 0) {
        if (released)
            m_held &= ~bit;
        else
            m_held |= bit;
        return 0;
    }
    if (released)
        return 0; // act on press

    if (key == Key::CAPS_LOCK) {
        m_caps = !m_caps;
        return 0;
    }
    if (key == Key::NUM_LOCK) {
        m_numLock = !m_numLock;
        return 0;
    }

    if (!m_numLock)
        key = NumpadToNav(key);

    for (const SpecialKey& s : kSpecialKeys) {
        if (s.key != key)
            continue;

        int mod = 1 + (Shift() ? 1 : 0) + (Alt() ? 2 : 0) + (Ctrl() ? 4 : 0);
        char* p = out;
        *p++ = '\x1b';
        if (s.form == Form::Tilde) {
            *p++ = '[';
            if (s.code >= 10)
                *p++ = '0' + s.code / 10;
            *p++ = '0' + s.code % 10;
            if (mod > 1) {
                *p++ = ';';
                *p++ = '0' + mod;
            }
            *p++ = '~';
        } else if (mod > 1) {
            *p++ = '[';
            *p++ = '1';
            *p++ = ';';
            *p++ = '0' + mod;
            *p++ = s.code;
        } else {
            *p++ = (s.form == Form::FnSS3 || m_appCursor) ? 'O' : '[';
            *p++ = s.code;
        }
        return p - out;
    }

    if (key == Key::TAB && Shift()) { // back-tab
        memcpy(out, "\x1b[Z", 3);
        return 3;
    }

    char c;
    if (!KeyToChar(key, c))
        return 0;
    if (Ctrl())
        c= ApplyCtrl(c);

    size_t n = 0;
    if (Alt())
        out[n++] = '\x1b';
    out[n++] = c;
    return n;
}

void KBDTranslator::SetApplicationCursorKeys(bool on) {
    m_appCursor = on;
}

bool KBDTranslator::Shift() const {
    return (m_held & (M_LSHIFT | M_RSHIFT)) > 0;
}

bool KBDTranslator::Ctrl() const {
    return (m_held & (M_LCTRL | M_RCTRL)) > 0;
}

bool KBDTranslator::Alt() const {
    return (m_held & (M_LALT | M_RALT)) > 0;
}

bool KBDTranslator::KeyToChar(Key k, char& out) const {
    switch (k) {
    case Key::ENTER:
    case Key::NP_ENTER:
        out = '\r';
        return true;
    case Key::TAB:
        out = '\t';
        return true;
    case Key::BACKSPACE:
        out = 0x7F;
        return true;
    case Key::ESC:
        out = 0x1B;
        return true;
    case Key::SPACE:
        out = ' ';
        return true;
    default:
        break;
    }

    const char* name = GetKeyName(k);
    if (name[0] == 0 || name[1] != 0) // printable characters only have 1 char
        return false;

    char c = name[0];
    if (c >= 'A' && c <= 'Z') {
        out = (Shift() != m_caps) ? c : static_cast<char>(c - 'A' + 'a');
        return true;
    }
    out = Shift() ? ShiftSymbol(c) : c; // also shifts numpad symbols
    return true;
}
