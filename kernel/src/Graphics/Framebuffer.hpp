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

#ifndef _FRAMEBUFFER_HPP
#define _FRAMEBUFFER_HPP

#include <stdint.h>

#include "Colour.hpp"

struct FrameBuffer {
    void* BaseAddress;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t red_mask;
    uint8_t red_shift;
    uint8_t green_mask;
    uint8_t green_shift;
    uint8_t blue_mask;
    uint8_t blue_shift;
};

void WriteToFrameBuffer(FrameBuffer* fb, uint64_t x, uint64_t y, Colour colour);
void WriteToFrameBuffer(FrameBuffer* fb, uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b);
void ClearFrameBuffer(FrameBuffer* fb, Colour colour);

#endif /* _FRAMEBUFFER_HPP */