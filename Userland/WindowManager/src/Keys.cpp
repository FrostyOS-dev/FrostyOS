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

#include "Keys.hpp"

#include <strings.h>

namespace WM {
    static const struct {
        const char* name;
        KeyCode key;
    } g_keyNames[] = {
#define WM_KEY_ENTRY(c, s) {#c, FOSI_KEY_##c},
        __FOSI_ENUMERATE_INPUT_KEYS(WM_KEY_ENTRY)
#undef WM_KEY_ENTRY
    };

    bool IsMouseButton(KeyCode k) {
        switch (k) {
        case FOSI_KEY_MOUSE_BTN_LEFT:
        case FOSI_KEY_MOUSE_BTN_RIGHT:
        case FOSI_KEY_MOUSE_BTN_MIDDLE:
        case FOSI_KEY_MOUSE_BTN_FOUR:
        case FOSI_KEY_MOUSE_BTN_FIVE:
            return true;
        default:
            return false;
        }
    }

    uint32_t ModifierBit(KeyCode k) {
        switch (k) {
        case FOSI_KEY_LEFT_SHIFT:
        case FOSI_KEY_RIGHT_SHIFT:
            return Mod::Shift;
        case FOSI_KEY_LEFT_CTRL:
        case FOSI_KEY_RIGHT_CTRL:
            return Mod::Ctrl;
        case FOSI_KEY_LEFT_ALT:
        case FOSI_KEY_RIGHT_ALT:
            return Mod::Alt;
        case FOSI_KEY_LEFT_SUPER:
        case FOSI_KEY_RIGHT_SUPER:
            return Mod::Super;
        default:
            return 0;
        }
    }

    bool KeyFromName(const std::string& name, KeyCode& out) {
        for (auto& e : g_keyNames) {
            if (strcasecmp(name.c_str(), e.name) == 0) {
                out = e.key;
                return true;
            }
        }
        return false;
    }

    const char* KeyIdentifier(KeyCode k) {
        if (k >= FOSI_KEY_COUNT)
            return "UNKNOWN";
        return g_keyNames[k].name;
    }
}
