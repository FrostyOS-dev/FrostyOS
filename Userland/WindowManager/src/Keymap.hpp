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

#ifndef _WM_KEYMAP_HPP
#define _WM_KEYMAP_HPP

#include <array>
#include <stdint.h>
#include <string>

#include "Keys.hpp"

namespace WM {
    struct KeyEntry {
        uint32_t level[4] = {};
        bool caps = false;
        bool numpad = false;
    };

    class Keymap {
    public:
        static const Keymap* Builtin(const std::string& name);
        
        uint32_t Lookup(KeyCode key, uint32_t mods, bool altGr) const;

    private:
        static Keymap BuildUS();
        void Set(KeyCode key, uint32_t normal, uint32_t shifted, bool caps = false);

        std::array<KeyEntry, KeyCount> m_entries;
    };
}

#endif /* _WM_KEYMAP_HPP */