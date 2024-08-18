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

#include "VGABackend.hpp"
#include "Graphics/VGAFont.hpp"

#include <Graphics/VGA.hpp>

#include <math.h>

TTYBackendVGA::TTYBackendVGA() : TTYBackend(TTYBackendType::VGA), m_vga(nullptr) {
    
}

TTYBackendVGA::TTYBackendVGA(VGA* vga) : TTYBackend(TTYBackendType::VGA), m_vga(vga) {
    
}

void TTYBackendVGA::Init(VGA* vga) {
    m_vga = vga;
}

void TTYBackendVGA::WriteChar(char c) {
    if (m_vga != nullptr)
        m_vga->PrintChar(c);
}

void TTYBackendVGA::WriteString(const char* str) {
    if (m_vga != nullptr)
        m_vga->PrintString(str);
}

void TTYBackendVGA::SetCursor(uint64_t x, uint64_t y) {
    if (m_vga != nullptr)
        m_vga->SetCursor(x * CHAR_WIDTH, y * CHAR_HEIGHT);
}

void TTYBackendVGA::GetCursor(uint64_t& x, uint64_t& y) {
    if (m_vga != nullptr) {
        uint64_t i_x, i_y;
        m_vga->GetCursor(i_x, i_y);
        x = i_x / CHAR_WIDTH;
        y = i_y / CHAR_HEIGHT;
    }
}

void TTYBackendVGA::Seek(uint64_t pos) {
    if (m_vga != nullptr) {
        uldiv_t div = uldiv(pos, m_vga->GetNumberOfColumns());
        m_vga->SetCursor(div.rem * CHAR_WIDTH, div.quot * CHAR_HEIGHT);
    }
}
