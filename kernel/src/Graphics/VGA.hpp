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

#ifndef _VGA_HPP
#define _VGA_HPP

#include <stdint.h>

#include "Colour.hpp"
#include "Framebuffer.hpp"

class VGA {
public:
    VGA();
    VGA(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour);

    void Init(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour);

    void PlotPixel(uint64_t x, uint64_t y, Colour colour);
    void ClearScreen(Colour colour);
    void DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);
    void DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);

    void SetFrameBuffer(FrameBuffer* framebuffer);
    void SetBackgroundColour(Colour& colour);
    void SetForegroundColour(Colour& colour);

    FrameBuffer* GetFrameBuffer() const;
    Colour GetBackgroundColour() const;
    Colour GetForegroundColour() const;

    void PrintChar(char c);
    void PrintString(const char* str);

    void Backspace();
    void NewLine();

    void Scroll(uint64_t n);
    
    void SetCursor(uint64_t x, uint64_t y);
    void GetCursor(uint64_t& x, uint64_t& y);

    uint64_t GetNumberOfRows();
    uint64_t GetNumberOfColumns();

private:
    FrameBuffer* m_framebuffer;
    Colour m_backgroundColour;
    Colour m_foregroundColour;

    uint64_t m_cursorX;
    uint64_t m_cursorY;
    uint64_t m_numberOfRows;
    uint64_t m_numberOfColumns;
};

#endif /* _VGA_HPP */