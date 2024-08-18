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

#ifndef _COLOUR_HPP
#define _COLOUR_HPP

#include <stdint.h>

class Colour
{
public:
    Colour();
    Colour(uint8_t r, uint8_t g, uint8_t b);

    uint64_t Render(uint16_t bpp, uint8_t red_mask, uint8_t red_shift, uint8_t green_mask, uint8_t green_shift, uint8_t blue_mask, uint8_t blue_shift);

    uint8_t GetR() const;
    uint8_t GetG() const;
    uint8_t GetB() const;

    void SetR(uint8_t r);
    void SetG(uint8_t g);
    void SetB(uint8_t b);

private:
    bool m_dirty;
    uint64_t m_colour;

    uint8_t m_r;
    uint8_t m_g;
    uint8_t m_b;
};

#endif