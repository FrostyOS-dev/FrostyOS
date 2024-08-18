/*
Copyright (©) 2022-2024  Frosty515

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

#ifndef _VGA_FONT_HPP
#define _VGA_FONT_HPP

#include <stdint.h>

#include "Framebuffer.hpp"

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 16

extern const uint8_t letters[95][16];

void getChar(const char in, uint8_t* out);

bool IsCharValid(char c);

void WriteCharToFrameBuffer(FrameBuffer* fb, uint64_t x, uint64_t y, Colour& fg, Colour& bg, char c);

#endif /* _KERNEL_X86_64_VGA_FONT_HPP */