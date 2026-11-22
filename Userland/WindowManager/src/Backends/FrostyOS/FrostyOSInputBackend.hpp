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

#ifndef _WM_FROSTYOS_INPUT_BACKEND_HPP
#define _WM_FROSTYOS_INPUT_BACKEND_HPP

#include "../InputDevice.hpp"

namespace WM {
    class FrostyOSInputBackend final : public InputBackend {
    public:
        explicit FrostyOSInputBackend(const char* dir = "/dev/input") : m_dir(dir) {}

        bool Initialise() override;

    private:
        std::string m_dir;
    };
}

#endif /* _WM_FROSTYOS_INPUT_BACKEND_HPP */