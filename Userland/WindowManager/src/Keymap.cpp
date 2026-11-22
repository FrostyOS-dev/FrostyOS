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

#include "Keymap.hpp"
#include "Keys.hpp"

#include <asm/key.h>

namespace WM {
    const Keymap* Keymap::Builtin(const std::string& name) {
        if (name == "us") {
            static const Keymap us = BuildUS();
            return &us;
        }
        return nullptr;
    }

    uint32_t Keymap::Lookup(KeyCode key, uint32_t mods, bool altGr) const {
        const KeyEntry& e = m_entries[key];
        if (e.numpad && (mods & Mod::NumLock) == 0)
            return 0;
        bool shift = (mods & Mod::Shift) != 0;
        if (e.caps && (mods & Mod::CapsLock) != 0)
            shift = !shift;
        return e.level[(shift ? 1 : 0) | (altGr ? 2 : 0)];
    }

    Keymap Keymap::BuildUS() {
        Keymap km;
        static const KeyCode letters[26] = {
            FOSI_KEY_A, FOSI_KEY_B, FOSI_KEY_C, FOSI_KEY_D, FOSI_KEY_E, FOSI_KEY_F, FOSI_KEY_G, FOSI_KEY_H, FOSI_KEY_I,
            FOSI_KEY_J, FOSI_KEY_K, FOSI_KEY_L, FOSI_KEY_M, FOSI_KEY_N, FOSI_KEY_O, FOSI_KEY_P, FOSI_KEY_Q, FOSI_KEY_R,
            FOSI_KEY_S, FOSI_KEY_T, FOSI_KEY_U, FOSI_KEY_V, FOSI_KEY_W, FOSI_KEY_X, FOSI_KEY_Y, FOSI_KEY_Z
        };
        for (int i = 0; i < 26; i++)
            km.Set(letters[i], 'a' + i, 'A' + i, true);

        static const KeyCode digits[10] = {FOSI_KEY_ONE, FOSI_KEY_TWO, FOSI_KEY_THREE, FOSI_KEY_FOUR, FOSI_KEY_FIVE,
                                           FOSI_KEY_SIX, FOSI_KEY_SEVEN, FOSI_KEY_EIGHT, FOSI_KEY_NINE, FOSI_KEY_ZERO};
        static const char plain[] = "1234567890";
        static const char shifted[] = "!@#$%^&*()";
        for (int i = 0; i < 10; i++)
            km.Set(digits[i], plain[i], shifted[i]);

        static const struct {
            KeyCode key;
            char normal, shifted;
        } punct[] = {
            {FOSI_KEY_BACKTICK, '`', '~'}, {FOSI_KEY_HYPHEN, '-', '_'}, {FOSI_KEY_EQUAL, '=', '+'},
            {FOSI_KEY_OPEN_BRACKET, '[', '{'}, {FOSI_KEY_CLOSE_BRACKET, ']', '}'}, {FOSI_KEY_BACKSLASH, '\\', '|'},
            {FOSI_KEY_SEMICOLON, ';', ':'}, {FOSI_KEY_APOSTROPHE, '\'', '"'},
            {FOSI_KEY_COMMA, ',', '<'}, {FOSI_KEY_DOT, '.', '>'}, {FOSI_KEY_FORWARD_SLASH, '/', '?'},
            {FOSI_KEY_SPACE, ' ', ' '},
            {FOSI_KEY_NP_FORWARD_SLASH, '/', '/'}, {FOSI_KEY_NP_ASTERISK, '*', '*'}, {FOSI_KEY_NP_HYPHEN, '-', '-'},
            {FOSI_KEY_NP_PLUS, '+', '+'}
        };
        for (auto& p : punct)
            km.Set(p.key, p.normal, p.shifted);

        static const KeyCode np[10] = {FOSI_KEY_NP_ZERO, FOSI_KEY_NP_ONE, FOSI_KEY_NP_TWO, FOSI_KEY_NP_THREE, FOSI_KEY_NP_FOUR, FOSI_KEY_NP_FIVE, FOSI_KEY_NP_SIX, FOSI_KEY_NP_SEVEN, FOSI_KEY_NP_EIGHT, FOSI_KEY_NP_NINE};
        for (int i = 0; i < 10; i++) {
            km.Set(np[i], '0' + i, '0' + i);
            km.m_entries[np[i]].numpad = true;
        }
        km.Set(FOSI_KEY_NP_DOT, '.', '.');
        km.m_entries[FOSI_KEY_NP_DOT].numpad = true;
        
        return km;
    }

    void Keymap::Set(KeyCode key, uint32_t normal, uint32_t shifted, bool caps) {
        KeyEntry& e = m_entries[key];
        e.level[0] = normal;
        e.level[1] = shifted;
        e.caps = caps;
    }

}