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

#include "FBConsole.hpp"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <Graphics/Colour.hpp>
#include <Graphics/Framebuffer.hpp>
#include <Graphics/VGAFont.hpp>

#include <HAL/drivers/Video/FBVideo/FBDisplay.hpp>
#include <HAL/drivers/Video/FBVideo/FBVideoDevice.hpp>

FBConsole::FBConsole() : m_display(nullptr), m_frameBuffer(nullptr), m_bg(), m_fg(), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_buffer(nullptr), m_oldCursorX(0), m_oldCursorY(0), m_fullFlush(false) {

}

FBConsole::FBConsole(Video::FBDisplay* display, Colour bg, Colour fg) : m_display(display), m_frameBuffer(nullptr), m_bg(bg), m_fg(fg), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_buffer(nullptr), m_oldCursorX(0), m_oldCursorY(0), m_fullFlush(false) {
    if (m_display != nullptr)
        m_frameBuffer = m_display->GetFrameBuffer();
}

FBConsole::~FBConsole() {

}

int FBConsole::Init() {
    if (m_display == nullptr)
        return -ENODEV;

    m_frameBuffer = m_display->GetFrameBuffer();

    m_numberOfRows = m_frameBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_frameBuffer->width / CHAR_WIDTH;

    m_cursorX = 0;
    m_cursorY = 0;
    m_oldCursorX = 0;
    m_oldCursorY = 0;

    ClearScreen(m_bg);
    return 0;
}

int FBConsole::Init(Video::FBDisplay* display, Colour bg, Colour fg) {
    m_display = display;
    m_bg = bg;
    m_fg = fg;

    return Init();
}

void FBConsole::ClearScreen() {
    ClearFrameBuffer(m_frameBuffer, m_bg);
}

void FBConsole::ClearScreen(Colour colour) {
    ClearFrameBuffer(m_frameBuffer, colour);
}

void FBConsole::PlotPixel(uint64_t x, uint64_t y, Colour colour) {
    WriteToFrameBuffer(m_frameBuffer, x, y, colour);
}

void FBConsole::DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
    // Draw the top and bottom lines
    for (uint64_t i = x; i < x + width; i++) {
        PlotPixel(i, y, colour);
        PlotPixel(i, y + height - 1, colour);
    }

    // Draw the left and right lines
    for (uint64_t i = y; i < y + height; i++) {
        PlotPixel(x, i, colour);
        PlotPixel(x + width - 1, i, colour);
    }
}

void FBConsole::DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
    for (uint64_t i = x; i < x + width; i++) {
        for (uint64_t j = y; j < y + height; j++) {
            PlotPixel(i, j, colour);
        }
    }
}

void FBConsole::SetBackgroundColour(Colour& colour) {
    m_bg = colour;
}

void FBConsole::SetForegroundColour(Colour& colour) {
    m_fg = colour;
}

Colour FBConsole::GetBackgroundColour() const {
    return m_bg;
}

Colour FBConsole::GetForegroundColour() const {
    return m_fg;
}

void FBConsole::PrintChar(char c) {
    switch (c) {
    case '\n':
    case '\v':
        NewLine();
        break;
    case '\b':
        Backspace();
        break;
    case '\a':
        break;
    case '\r':
        m_cursorX = 0;
        break;
    case '\t':
        for (uint64_t i = 0; i < 4; i++)
            PrintChar(' ');
        break;
    case '\f':
        // ClearScreen(m_bg);
        memset(m_buffer, ' ', m_numberOfColumns * m_numberOfRows);
        m_cursorX = 0;
        m_cursorY = 0;
        break;
    default:
        if (c >= ' ' && c < 0x7F) {
            m_buffer[(m_cursorY / CHAR_HEIGHT) * m_numberOfColumns + (m_cursorX / CHAR_WIDTH)] = c;
            m_cursorX += CHAR_WIDTH;

            if (m_cursorX >= (m_numberOfColumns * CHAR_WIDTH))
                NewLine();
        }
        break;
    }
}

void FBConsole::Backspace() {
    if (m_cursorX == 0) {
        if (m_cursorY > 0) {
            m_cursorY -= CHAR_HEIGHT;
            m_cursorX = (m_numberOfColumns - 1) * CHAR_WIDTH;
        }
    }
    else
        m_cursorX -= CHAR_WIDTH;

    m_buffer[(m_cursorY / CHAR_HEIGHT) * m_numberOfColumns + (m_cursorX / CHAR_WIDTH)] = ' ';
}

void FBConsole::NewLine() {
    m_cursorX = 0;
    m_cursorY += CHAR_HEIGHT;

    if (m_cursorY >= (m_numberOfRows * CHAR_HEIGHT))
        Scroll(1);
}

void FBConsole::Scroll(uint64_t n) {
    m_cursorY -= n * CHAR_HEIGHT;
    memmove(m_buffer, (void*)((uint64_t)m_buffer + n * m_numberOfColumns), (m_cursorY / CHAR_HEIGHT) * m_numberOfColumns);
    memset((void*)((uint64_t)m_buffer + (m_cursorY / CHAR_HEIGHT) * m_numberOfColumns), ' ', n * m_numberOfColumns);
    m_fullFlush = true;
}

void FBConsole::SetCursor(uint64_t x, uint64_t y) {
    m_cursorX = x;
    m_cursorY = y;
}

void FBConsole::GetCursor(uint64_t& x, uint64_t& y) {
    x = m_cursorX;
    y = m_cursorY;
}

uint64_t FBConsole::GetNumberOfRows() {
    return m_numberOfRows;
}

uint64_t FBConsole::GetNumberOfColumns() {
    return m_numberOfColumns;
}

uint64_t FBConsole::GetWidth() {
    return m_frameBuffer->width;
}

uint64_t FBConsole::GetHeight() {
    return m_frameBuffer->height;
}

FrameBuffer* FBConsole::GetFrameBuffer() {
    return m_frameBuffer;
}

void FBConsole::CopyFrom(FBConsole* other) {
    m_cursorX = other->m_cursorX;
    m_cursorY = other->m_cursorY;
    m_oldCursorX = other->m_oldCursorX;
    m_oldCursorY = other->m_oldCursorY;

    size_t rows = MIN(other->m_numberOfRows, m_numberOfRows);
    size_t columns = MIN(other->m_numberOfColumns, m_numberOfColumns);

    for (uint64_t i = 0; i < rows; i++) {
        memcpy((void*)((uint64_t)m_buffer + i * m_numberOfColumns), (void*)((uint64_t)other->m_buffer + i * other->m_numberOfColumns), columns);
        if ((m_numberOfColumns - columns) > 0)
            memset((void*)((uint64_t)m_buffer + i * m_numberOfColumns + columns), ' ', (m_numberOfColumns - columns));
    }

    if ((m_numberOfRows - rows) > 0)
        memset((void*)((uint64_t)m_buffer + rows * m_numberOfColumns), ' ', (m_numberOfRows - rows) * m_numberOfColumns);
}

void FBConsole::Flush() {
    if (m_fullFlush || m_oldCursorY > m_cursorY || (m_oldCursorY == m_cursorY && m_oldCursorX > m_cursorX)) {
        for (uint64_t y = 0; y < m_numberOfRows; y++) {
            for (uint64_t x = 0; x < m_numberOfColumns; x++)
                WriteCharToFrameBuffer(m_frameBuffer, x * CHAR_WIDTH, y * CHAR_HEIGHT, m_fg, m_bg, m_buffer[y * m_numberOfColumns + x]);
        }
        m_fullFlush = false;
    } else if (m_oldCursorY < m_cursorY) {
        // Step 1: Flush what remains of the first row
        uint64_t y = m_oldCursorY / CHAR_HEIGHT;
        if (m_oldCursorX > 0) {
            for (uint64_t x = m_cursorX / CHAR_WIDTH; x < m_numberOfColumns; x++)
                WriteCharToFrameBuffer(m_frameBuffer, x * CHAR_WIDTH, y * CHAR_HEIGHT, m_fg, m_bg, m_buffer[y * m_numberOfColumns + x]);
            y++;
        }

        // Step 2: Flush all the rows in between
        for (; y < (m_cursorY / CHAR_HEIGHT); y++) {
            for (uint64_t x = 0; x < m_numberOfColumns; x++)
                WriteCharToFrameBuffer(m_frameBuffer, x * CHAR_WIDTH, y * CHAR_HEIGHT, m_fg, m_bg, m_buffer[y * m_numberOfColumns + x]);
        }

        // Step 3: Flush the first part of the remaining row
        if (m_cursorX > 0) {
            for (uint64_t x = 0; x < (m_cursorX / CHAR_WIDTH); x++)
                WriteCharToFrameBuffer(m_frameBuffer, x * CHAR_WIDTH, y * CHAR_HEIGHT, m_fg, m_bg, m_buffer[y * m_numberOfColumns + x]);
        }
    } else if (m_oldCursorX < m_cursorX) {
        for (uint64_t x = m_oldCursorX; x < m_cursorX; x += CHAR_WIDTH)
            WriteCharToFrameBuffer(m_frameBuffer, x, m_cursorY, m_fg, m_bg, m_buffer[(m_cursorY / CHAR_HEIGHT) * m_numberOfColumns + (x / CHAR_WIDTH)]);
    }
    
    m_oldCursorX = m_cursorX;
    m_oldCursorY = m_cursorY;
}

void FBConsole::SetBuffer(char* buf, size_t rows, size_t columns) {
    m_buffer = buf;
    m_numberOfRows = MIN(rows, m_numberOfRows);
    m_numberOfColumns = MIN(columns, m_numberOfColumns);
}

void FBConsole::CreateBuffer() {
    m_numberOfRows = m_frameBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_frameBuffer->width / CHAR_WIDTH;

    m_buffer = new char[m_numberOfRows * m_numberOfColumns];
}


FBConsole* g_KFBConsole = nullptr;

FBConsole KEarlyFBConsole;
Video::FBDisplay KEarlyFBDisplay(UINT64_MAX);
Video::FBVideoDevice KEarlyFBDevice;

#define EARLY_MAX_WIDTH 1024
#define EARLY_MAX_HEIGHT 768

char KEarlyFBBuffer[(EARLY_MAX_WIDTH / CHAR_WIDTH) * (EARLY_MAX_HEIGHT / CHAR_HEIGHT)];

extern Colour g_KBackgroundColour;
extern Colour g_KForegroundColour;

int FBConsole_EarlyInit(FrameBuffer* fb) {
    KEarlyFBDisplay.SetFrameBuffer(fb);
    KEarlyFBDevice.SetDisplay(&KEarlyFBDisplay);
    int rc = KEarlyFBDevice.Init();
    if (rc < 0)
        return rc;

    rc = KEarlyFBConsole.Init(&KEarlyFBDisplay, g_KBackgroundColour, g_KForegroundColour);
    if (rc < 0)
        return rc;
    
    memset(KEarlyFBBuffer, ' ', (EARLY_MAX_WIDTH / CHAR_WIDTH) * (EARLY_MAX_HEIGHT / CHAR_HEIGHT));
    KEarlyFBConsole.SetBuffer(KEarlyFBBuffer, (EARLY_MAX_HEIGHT / CHAR_HEIGHT), (EARLY_MAX_WIDTH / CHAR_WIDTH));
    
    g_KFBConsole = &KEarlyFBConsole;

    return 0;
}

int FBConsole_FullInit(Video::FBDisplay* display, FBConsole** consoleOut) {
    if (display == nullptr)
        return -EINVAL;

    FBConsole* console = new FBConsole(display, g_KBackgroundColour, g_KForegroundColour);
    console->CreateBuffer();
    console->CopyFrom(g_KFBConsole);

    g_KFBConsole = console;

    if (consoleOut != nullptr)
        *consoleOut = g_KFBConsole;

    return 0;
}
