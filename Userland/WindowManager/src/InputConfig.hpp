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

#ifndef _WM_INPUT_CONFIG_HPP
#define _WM_INPUT_CONFIG_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Keys.hpp"

namespace WM {
    struct KeyboardConfig {
        std::string layout = "us";
        bool numLockOnStart = true;
    };

    enum class AccelProfile {
        Flat,
        Adaptive
    };

    struct PointerConfig {
        AccelProfile accel = AccelProfile::Adaptive;
        double speed = 0.0; // -1 to 1, exponential: 2 ^ speed
        bool naturalScroll = false;
        bool leftHanded = false;
    };

    struct Binding {
        uint32_t mods = 0;
        KeyCode key = 0;
        std::string action;
    };

    struct InputConfig {
        KeyboardConfig keyboard;
        PointerConfig pointer;
        std::vector<Binding> bindings;

        static InputConfig Defaults();
        
        bool AddBinding(const std::string& combo, const std::string& action, std::string* error = nullptr);
    };

    bool ParseCombo(const std::string& combo, uint32_t& mods, KeyCode& key, std::string& error);

    class ActionRegistry {
    public:
        void Register(const std::string& name, std::function<void()> fn) {
            m_actions[name] = std::move(fn);
        }

        bool Run(const std::string& name) const {
            auto it = m_actions.find(name);
            if (it == m_actions.end())
                return false;
            it->second();
            return true;
        }

    private:
        std::unordered_map<std::string, std::function<void()>> m_actions;
    };
}

#endif /* _WM_INPUT_CONFIG_HPP */