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

#include "Colour.hpp"

Colour::Colour() : m_dirty(true), m_colour(0), m_r(0), m_g(0), m_b(0) {

}

Colour::Colour(uint8_t r, uint8_t g, uint8_t b) : m_dirty(true), m_colour(0), m_r(r), m_g(g), m_b(b) {

}

uint64_t Colour::Render(uint16_t bpp, uint8_t red_mask, uint8_t red_shift, uint8_t green_mask, uint8_t green_shift, uint8_t blue_mask, uint8_t blue_shift) {
    if (!m_dirty)
        return m_colour;
    m_colour = (((uint64_t)(m_r & red_mask) << red_shift)
                | ((uint64_t)(m_g & green_mask) << green_shift)
                | ((uint64_t)(m_b & blue_mask) << blue_shift))
               & (0xFFFFFFFFFFFFFFFF >> (64 - bpp));
    m_dirty = false;
    return m_colour;
}
