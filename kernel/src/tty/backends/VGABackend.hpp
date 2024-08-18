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

#ifndef _TTY_BACKEND_VGA_HPP
#define _TTY_BACKEND_VGA_HPP

#include "../TTYBackend.hpp"

#include <Graphics/VGA.hpp>

class TTYBackendVGA : public TTYBackend {
public:
    TTYBackendVGA();
    TTYBackendVGA(VGA* vga);

    void Init(VGA* vga);

    void WriteChar(char c) override;
    void WriteString(const char* str) override;

    void SetCursor(uint64_t x, uint64_t y) override;
    void GetCursor(uint64_t& x, uint64_t& y) override;

    void Seek(uint64_t pos) override;
private:
    VGA* m_vga;
};

#endif /* _TTY_BACKEND_VGA_HPP */