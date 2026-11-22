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

#ifndef _WM_CURSOR_HPP
#define _WM_CURSOR_HPP

#include <stdint.h>
#include <vector>

namespace WM {
    constexpr int CursorWidth = 12;
    constexpr int CursorHeight = 19;

    inline std::vector<uint32_t> BuildCursorPixels() {
        static const char* const art[CursorHeight] = {
            "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ", "X.....X     ",
            "X......X    ", "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ", "X.X X..X    ", "XX  X..X    ",
            "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ", "       XX   "};

        std::vector<uint32_t> px(CursorWidth * CursorHeight, 0);
        for (int y = 0; y < CursorHeight; y++) {
            for (int x = 0; x < CursorWidth; x++) {
                char c = art[y][x];
                px[y * CursorWidth + x] = c == 'X' ? 0xFF000000u : c == '.' ? 0xFFFFFFFFu : 0u; // premultiplied ARGB
            }
        }
        return px;
    }
}

#endif /* _WM_CURSOR_HPP */