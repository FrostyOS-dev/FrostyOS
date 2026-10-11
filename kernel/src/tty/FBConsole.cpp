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

FBConsole::FBConsole() : m_display(nullptr), m_frameBuffer(nullptr), m_bg(), m_fg(), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_buffer(nullptr), m_wrapPending(false), m_scrollTop(0), m_scrollBottom(0) {

}

FBConsole::FBConsole(Video::FBDisplay* display, Colour bg, Colour fg) : m_display(display), m_frameBuffer(nullptr), m_bg(bg), m_fg(fg), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_buffer(nullptr), m_wrapPending(false), m_scrollTop(0), m_scrollBottom(0) {
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

    ResetScrollRegion();

    m_cursorX = 0;
    m_cursorY = 0;

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
    ClearScreen(m_bg);
}

void FBConsole::ClearScreen(Colour colour) {
    ClearFrameBuffer(m_frameBuffer, colour);
    if (m_buffer == nullptr)
        return;
    for (size_t i = 0; i < m_numberOfRows * m_numberOfColumns; i++) {
        FBConsoleCell& c = m_buffer[i];
        c.ch = ' ';
        c.fgR = m_fg.GetR();
        c.fgG = m_fg.GetG();
        c.fgB = m_fg.GetB();
        c.bgR = colour.GetR();
        c.bgG = colour.GetG();
        c.bgB = colour.GetB();
        c.dirty = false; // pixels already match
    }
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
        m_wrapPending = false;
        break;
    case '\t': {
        if (m_numberOfColumns == 0)
            break;
        uint64_t next = (CursorCol() + 8) & ~UINT64_C(7);
        if (next >= m_numberOfColumns)
            next = m_numberOfColumns - 1;
        m_cursorX = next * CHAR_WIDTH;
        m_wrapPending = false;
        break;
    }
    case '\f':
        BlankCells(0, m_numberOfColumns * m_numberOfRows);
        m_cursorX = 0;
        m_cursorY = 0;
        m_wrapPending = false;
        break;
    default:
        if (c >= ' ' && c < 0x7F && m_numberOfColumns > 0 && m_numberOfRows > 0) {
            if (m_wrapPending)
                NewLine();

            SetCell(CursorRow() * m_numberOfColumns + CursorCol(), c);

            if (CursorCol() + 1 >= m_numberOfColumns) // Deferred wrap
                m_wrapPending = true;
            else
                m_cursorX += CHAR_WIDTH;
        }
        break;
    }
}

void FBConsole::Backspace() {
    if (m_wrapPending) {
        m_wrapPending = false;
        return;
    }

    if (m_cursorX == 0) {
        if (m_cursorY > 0) {
            m_cursorY -= CHAR_HEIGHT;
            m_cursorX = (m_numberOfColumns - 1) * CHAR_WIDTH;
        }
    } else
        m_cursorX -= CHAR_WIDTH;
}

void FBConsole::NewLine() {
    m_cursorX = 0;
    LineFeed();
}

void FBConsole::LineFeed() {
    m_wrapPending = false;
    uint64_t row = CursorRow();
    if (row == m_scrollBottom)
        ScrollRows(m_scrollTop, m_scrollBottom, 1, true);
    else if (row + 1 < m_numberOfRows)
        m_cursorY = (row + 1) * CHAR_HEIGHT;
}

void FBConsole::ReverseLineFeed() {
    m_wrapPending = false;
    uint64_t row = CursorRow();
    if (row == m_scrollTop)
        ScrollRows(m_scrollTop, m_scrollBottom, 1, false);
    else if (row > 0)
        m_cursorY = (row - 1) * CHAR_HEIGHT;
}

void FBConsole::Scroll(uint64_t n) {
    ScrollRows(m_scrollTop, m_scrollBottom, n, true);
}

void FBConsole::ScrollDown(uint64_t n) {
    ScrollRows(m_scrollTop, m_scrollBottom, n, false);
}

bool FBConsole::SetScrollRegion(uint64_t top, uint64_t bottom) {
    if (m_numberOfRows == 0)
        return false;
    if (bottom >= m_numberOfRows)
        bottom = m_numberOfRows - 1;
    if (top >= bottom)
        return false; // must be at least 2 rows
    m_scrollTop = top;
    m_scrollBottom = bottom;
    return true;
}

void FBConsole::InsertLines(uint64_t n) {
    uint64_t row = CursorRow();
    if (row < m_scrollTop || row > m_scrollBottom)
        return;
    ScrollRows(row, m_scrollBottom, n, false);
    m_cursorX = 0;
    m_wrapPending = false;
}

void FBConsole::DeleteLines(uint64_t n) {
    uint64_t row = CursorRow();
    if (row < m_scrollTop || row > m_scrollBottom)
        return;
    ScrollRows(row, m_scrollBottom, n, true);
    m_cursorX = 0;
    m_wrapPending = false;
}

void FBConsole::InsertChars(uint64_t n) {
    if (m_buffer == nullptr || m_numberOfColumns == 0 || m_numberOfRows == 0)
        return;
    m_wrapPending = false;

    uint64_t cols = m_numberOfColumns;
    uint64_t col = CursorCol();
    uint64_t row = CursorRow();
    FBConsoleCell* line = m_buffer + row * cols;

    n = MIN(n, cols - col);
    memmove(line + col + n, line + col, (cols - col - n) * sizeof(FBConsoleCell));
    for (uint64_t i = col + n; i < cols; i++)
        line[i].dirty = true;
    BlankCells(row * cols + col, n);
}

void FBConsole::DeleteChars(uint64_t n) {
    if (m_buffer == nullptr || m_numberOfColumns == 0 || m_numberOfRows == 0)
        return;
    m_wrapPending = false;

    uint64_t cols = m_numberOfColumns;
    uint64_t col = CursorCol();
    uint64_t row = CursorRow();
    FBConsoleCell* line = m_buffer + row * cols;

    n = MIN(n, cols - col);
    uint64_t moved = cols - col - n;
    memmove(line + col, line + col + n, moved * sizeof(FBConsoleCell));
    for (uint64_t i = col; i < col + moved; i++)
        line[i].dirty = true;
    BlankCells(row * cols + cols - n, n);
}

void FBConsole::EraseChars(uint64_t n) {
    if (m_buffer == nullptr || m_numberOfColumns == 0 || m_numberOfRows == 0)
        return;
    m_wrapPending = false;

    uint64_t col = CursorCol();
    n = MIN(n, m_numberOfColumns - col);
    BlankCells(CursorRow() * m_numberOfColumns + col, n);
}

void FBConsole::SetCursor(uint64_t x, uint64_t y) {
    if (m_numberOfColumns > 0 && m_numberOfRows > 0) {
        x = MIN(x, (m_numberOfColumns - 1) * CHAR_WIDTH);
        y = MIN(y, (m_numberOfRows - 1) * CHAR_HEIGHT);
    }

    m_cursorX = x;
    m_cursorY = y;
    m_wrapPending = false;
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
    SetCursor(other->m_cursorX, other->m_cursorY);

    size_t rows = MIN(other->m_numberOfRows, m_numberOfRows);
    size_t cols = MIN(other->m_numberOfColumns, m_numberOfColumns);

    BlankCells(0, m_numberOfRows * m_numberOfColumns);
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            m_buffer[r * m_numberOfColumns + c] = other->m_buffer[r * other->m_numberOfColumns + c];
            m_buffer[r * m_numberOfColumns + c].dirty = true;
        }
    }
}

void FBConsole::Flush() {
    if (m_buffer == nullptr)
        return;

    for (uint64_t y = 0; y < m_numberOfRows; y++) {
        for (uint64_t x = 0; x < m_numberOfColumns; x++) {
            FBConsoleCell& c = m_buffer[y * m_numberOfColumns + x];
            if (!c.dirty)
                continue;
            Colour fg(c.fgR, c.fgG, c.fgB);
            Colour bg(c.bgR, c.bgG, c.bgB);
            WriteCharToFrameBuffer(m_frameBuffer, x * CHAR_WIDTH, y * CHAR_HEIGHT, fg, bg, c.ch);
            c.dirty = false;
        }
    }
}

void FBConsole::SetBuffer(FBConsoleCell* buf, size_t rows, size_t columns) {
    m_buffer = buf;
    m_numberOfRows = MIN(rows, m_numberOfRows);
    m_numberOfColumns = MIN(columns, m_numberOfColumns);
    BlankCells(0, m_numberOfRows * m_numberOfColumns);
    ResetScrollRegion();
}

void FBConsole::CreateBuffer() {
    m_numberOfRows = m_frameBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_frameBuffer->width / CHAR_WIDTH;

    m_buffer = new FBConsoleCell[m_numberOfRows * m_numberOfColumns];
    BlankCells(0, m_numberOfRows * m_numberOfColumns);
    ResetScrollRegion();
}

void FBConsole::EraseInLine(int mode) {
    if (m_buffer == nullptr || m_numberOfRows == 0 || m_numberOfColumns == 0)
        return;

    size_t row = MIN(m_cursorY / CHAR_HEIGHT, m_numberOfRows - 1);
    size_t col = MIN(m_cursorX / CHAR_WIDTH, m_numberOfColumns - 1);
    size_t start = row * m_numberOfColumns;
    switch (mode) {
    case 0:
        BlankCells(start + col, m_numberOfColumns - col);
        break;
    case 1:
        BlankCells(start, col + 1);
        break;
    case 2:
        BlankCells(start, m_numberOfColumns);
        break;
    }
}

void FBConsole::EraseInDisplay(int mode) {
    if (m_buffer == nullptr || m_numberOfRows == 0 || m_numberOfColumns == 0)
        return;

    size_t row = MIN(m_cursorY / CHAR_HEIGHT, m_numberOfRows - 1);
    size_t col = MIN(m_cursorX / CHAR_WIDTH, m_numberOfColumns - 1);
    size_t cur = row * m_numberOfColumns + col;
    size_t total = m_numberOfRows * m_numberOfColumns;
    switch (mode) {
    case 0:
        BlankCells(cur, total - cur);
        break;
    case 1:
        BlankCells(0, cur + 1);
        break;
    case 2:
    case 3:
        BlankCells(0, total);
        break;
    }
}

void FBConsole::SetCell(size_t i, char c) {
    if (m_buffer == nullptr || i >= m_numberOfRows * m_numberOfColumns)
        return;

    FBConsoleCell& cell = m_buffer[i];
    cell.ch = c;
    cell.fgR = m_fg.GetR();
    cell.fgG = m_fg.GetG();
    cell.fgB = m_fg.GetB();
    cell.bgR = m_bg.GetR();
    cell.bgG = m_bg.GetG();
    cell.bgB = m_bg.GetB();
    cell.dirty = true;
}

void FBConsole::BlankCells(size_t start, size_t count) {
    for (size_t i = 0; i < count; i++)
        SetCell(start + i, ' ');
}

void FBConsole::ScrollRows(uint64_t top, uint64_t bottom, uint64_t n, bool up) {
    if (m_buffer == nullptr || n == 0 || m_numberOfColumns == 0 || bottom >= m_numberOfRows || top > bottom)
        return;

    uint64_t cols = m_numberOfColumns;
    uint64_t height = bottom - top + 1;
    if (n > height)
        n = height;
    uint64_t moved = height - n;
    FBConsoleCell* base = m_buffer + top * cols;

    if (up) {
        memmove(base, base + n * cols, moved * cols * sizeof(FBConsoleCell));
        for (uint64_t i = 0; i < moved * cols; i++)
            base[i].dirty = true;
        BlankCells((top + moved) * cols, n * cols);
    } else {
        memmove(base + n * cols, base, moved * cols * sizeof(FBConsoleCell));
        for (uint64_t i = n * cols; i < height * cols; i++)
            base[i].dirty = true;
        BlankCells(top * cols, n * cols);
    }
}

void FBConsole::ResetScrollRegion() {
    m_scrollTop = 0;
    m_scrollBottom = m_numberOfRows > 0 ? m_numberOfRows - 1 : 0;
}

uint64_t FBConsole::CursorRow() const {
    return m_numberOfRows > 0 ? MIN(m_cursorY / CHAR_HEIGHT, m_numberOfRows - 1) : 0;
}

uint64_t FBConsole::CursorCol() const {
    return m_numberOfColumns > 0 ? MIN(m_cursorX / CHAR_WIDTH, m_numberOfColumns - 1) : 0;
}


FBConsole* g_KFBConsole = nullptr;

FBConsole KEarlyFBConsole;
Video::FBDisplay KEarlyFBDisplay(UINT64_MAX);
Video::FBVideoDevice KEarlyFBDevice;

#define EARLY_MAX_WIDTH 1024
#define EARLY_MAX_HEIGHT 768

FBConsoleCell KEarlyFBBuffer[(EARLY_MAX_WIDTH / CHAR_WIDTH) * (EARLY_MAX_HEIGHT / CHAR_HEIGHT)];

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
