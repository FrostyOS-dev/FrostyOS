/*
Copyright (©) 2024  Frosty515

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

#ifndef _TTY_DEBUG_BACKEND_HPP
#define _TTY_DEBUG_BACKEND_HPP

#include "../TTYBackend.hpp"

class TTYBackendDebug : public TTYBackend {
public:
    TTYBackendDebug();

    void WriteChar(char c) override;
    void WriteString(const char* str) override;
};

#endif /* _TTY_DEBUG_BACKEND_HPP */