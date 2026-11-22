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

#include "InputConfig.hpp"
#include "Keys.hpp"

#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace WM {
    static std::string Trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t");
        return s.substr(a, b - a + 1);
    }

    static uint32_t ModifierFromWord(const std::string& w) {
        if (strcasecmp(w.c_str(), "shift") == 0)
            return Mod::Shift;
        if (strcasecmp(w.c_str(), "ctrl") == 0 || strcasecmp(w.c_str(), "control") == 0)
            return Mod::Ctrl;
        if (strcasecmp(w.c_str(), "alt") == 0)
            return Mod::Alt;
        if (strcasecmp(w.c_str(), "super") == 0 || strcasecmp(w.c_str(), "logo") == 0 || strcasecmp(w.c_str(), "win") == 0)
            return Mod::Super;
        return 0;
    }

    bool ParseCombo(const std::string& combo, uint32_t& mods, KeyCode& key, std::string& error) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t plus = combo.find('+', start);
            parts.push_back(Trim(combo.substr(start, plus == std::string::npos ? std::string::npos : plus - start)));
            if (plus == std::string::npos)
                break;
            start = plus + 1;
        }

        mods = 0;
        for (size_t i = 0; i < parts.size(); i++) {
            if (parts[i].empty()) {
                error = "empty element in '" + combo + "'";
                return false;
            }
            if (i + 1 < parts.size()) {
                uint32_t m = ModifierFromWord(parts[i]);
                if (m == 0) {
                    error = "unknown modifier '" + parts[i] + "'";
                    return false;
                }
                mods |= m;
            } else if (!KeyFromName(parts[i], key)) {
                error = "unknown key '" + parts[i] + "'";
                return false;
            }
        }
        return true;
    }

    bool InputConfig::AddBinding(const std::string& combo, const std::string& action, std::string* error) {
        Binding b;
        std::string err;
        if (!ParseCombo(combo, b.mods, b.key, err)) {
            if (error != nullptr)
                *error = err;
            return false;
        }
        b.action = action;
        bindings.push_back(std::move(b));
        return true;
    }

    InputConfig InputConfig::Defaults() {
        InputConfig c;
        const struct {
            const char* combo;
            const char* action;
        } defaults[] = {
            {"Ctrl+Alt+BACKSPACE", "quit"},
            {"Alt+TAB", "focus-next"}
        };
        for (auto& d : defaults) {
            std::string err;
            if (!c.AddBinding(d.combo, d.action, &err))
                fprintf(stderr, "WindowManager: bad default binding '%s': %s\n", d.combo, err.c_str());
        }
        return c;
    }
}