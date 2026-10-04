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

#ifndef _TTY_FBCONSOLE_HPP
#define _TTY_FBCONSOLE_HPP

#include <stdint.h>

#include <Graphics/Colour.hpp>
#include <Graphics/Framebuffer.hpp>

#include <HAL/drivers/Video/FBVideo/FBDisplay.hpp>

class FBConsole {
public:
    FBConsole();
    FBConsole(Video::FBDisplay* display, Colour bg, Colour fg);
    ~FBConsole();

    int Init();
    int Init(Video::FBDisplay* display, Colour bg, Colour fg);

    void ClearScreen();
    void ClearScreen(Colour colour);

    void PlotPixel(uint64_t x, uint64_t y, Colour colour);
    void DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);
    void DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);

    void SetBackgroundColour(Colour& colour);
    void SetForegroundColour(Colour& colour);

    Colour GetBackgroundColour() const;
    Colour GetForegroundColour() const;

    void PrintChar(char c);
    
    void Backspace();
    void NewLine();

    void Scroll(uint64_t n);

    void SetCursor(uint64_t x, uint64_t y);
    void GetCursor(uint64_t& x, uint64_t& y);

    uint64_t GetNumberOfRows();
    uint64_t GetNumberOfColumns();

    uint64_t GetWidth();
    uint64_t GetHeight();

    FrameBuffer* GetFrameBuffer();

    void CopyFrom(FBConsole* other); // copy cursor position and buffer contents

    void Flush();

    void SetBuffer(char* buf, size_t rows, size_t columns); // set the buffer, and set the screen size to the requested
    void CreateBuffer(); // Create a buffer that is the appropriate size for the display dimensions

private:
    Video::FBDisplay* m_display;
    FrameBuffer* m_frameBuffer;
    Colour m_bg;
    Colour m_fg;

    uint64_t m_cursorX;
    uint64_t m_cursorY;
    uint64_t m_numberOfRows;
    uint64_t m_numberOfColumns;

    char* m_buffer;
    uint64_t m_oldCursorX;
    uint64_t m_oldCursorY;
    bool m_fullFlush;
};

extern FBConsole* g_KFBConsole;

int FBConsole_EarlyInit(FrameBuffer* fb);
int FBConsole_FullInit(Video::FBDisplay* display, FBConsole** consoleOut = nullptr);

#endif /* _TTY_FBCONSOLE_HPP */