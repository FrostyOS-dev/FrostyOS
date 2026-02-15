/*
Copyright (©) 2024-2026  Frosty515

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
#include "Framebuffer.hpp"
#include "VGA.hpp"
#include "VGAFont.hpp"

#include <string.h>
#include <util.h>

VGA::VGA() : m_frontBuffer(nullptr), m_backBuffer(nullptr), m_backgroundColour(0, 0, 0), m_foregroundColour(255, 255, 255), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_doubleBufferingEnabled(false) {

}

VGA::VGA(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour) : m_frontBuffer(framebuffer), m_backBuffer(framebuffer), m_backgroundColour(backgroundColour), m_foregroundColour(foregroundColour), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_doubleBufferingEnabled(false) {
    m_numberOfRows = m_backBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_backBuffer->width / CHAR_WIDTH;
}

void VGA::Init(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour) {
    m_backBuffer = framebuffer;
    m_frontBuffer = framebuffer;
    m_backgroundColour = backgroundColour;
    m_foregroundColour = foregroundColour;

    m_numberOfRows = m_backBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_backBuffer->width / CHAR_WIDTH;

    m_cursorX = 0;
    m_cursorY = 0;

    ClearScreen(m_backgroundColour);
}

void VGA::PlotPixel(uint64_t x, uint64_t y, Colour colour) {
    WriteToFrameBuffer(m_backBuffer, x, y, colour);
}

void VGA::ClearScreen(Colour colour) {
    ClearFrameBuffer(m_backBuffer, colour);
}

void VGA::DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
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

void VGA::DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
    for (uint64_t i = x; i < x + width; i++) {
        for (uint64_t j = y; j < y + height; j++) {
            PlotPixel(i, j, colour);
        }
    }
}

void VGA::SetFrameBuffer(FrameBuffer* framebuffer) {
    m_backBuffer = framebuffer;
}

void VGA::SetBackgroundColour(Colour& colour) {
    m_backgroundColour = colour;
}

void VGA::SetForegroundColour(Colour& colour) {
    m_foregroundColour = colour;
}

FrameBuffer* VGA::GetFrameBuffer() const {
    return m_backBuffer;
}

Colour VGA::GetBackgroundColour() const {
    return m_backgroundColour;
}

Colour VGA::GetForegroundColour() const {
    return m_foregroundColour;
}

void VGA::PrintChar(char c) {
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
        ClearScreen(m_backgroundColour);
        m_cursorX = 0;
        m_cursorY = 0;
        break;
    default:
        WriteCharToFrameBuffer(m_backBuffer, m_cursorX, m_cursorY, m_foregroundColour, m_backgroundColour, c);
        m_cursorX += CHAR_WIDTH;

        if (m_cursorX >= (m_numberOfColumns * CHAR_WIDTH))
            NewLine();
        break;
    }
}

void VGA::PrintString(const char* str) {
    for (uint64_t i = 0; str[i] != '\0'; i++) {
        PrintChar(str[i]);
    }
}

void VGA::Backspace() {
    if (m_cursorX == 0) {
        if (m_cursorY > 0) {
            m_cursorY -= CHAR_HEIGHT;
            m_cursorX = (m_numberOfColumns - 1) * CHAR_WIDTH;
        }
    }
    else
        m_cursorX -= CHAR_WIDTH;

    DrawFilledRectangle(m_cursorX, m_cursorY, CHAR_WIDTH, CHAR_HEIGHT, m_backgroundColour);
}

void VGA::NewLine() {
    m_cursorX = 0;
    m_cursorY += CHAR_HEIGHT;

    if (m_cursorY >= (m_numberOfRows * CHAR_HEIGHT))
        Scroll(1);
}

void VGA::Scroll(uint64_t n) {
    memcpy(m_backBuffer->BaseAddress, (void*)((uint64_t)m_backBuffer->BaseAddress + n * m_backBuffer->pitch * CHAR_HEIGHT), m_backBuffer->pitch * (m_backBuffer->height - n * CHAR_HEIGHT));
    DrawFilledRectangle(0, m_backBuffer->height - n * CHAR_HEIGHT, m_backBuffer->width, n * CHAR_HEIGHT, m_backgroundColour);
    m_cursorY -= n * CHAR_HEIGHT;
}

void VGA::SetCursor(uint64_t x, uint64_t y) {
    m_cursorX = x;
    m_cursorY = y;
}

void VGA::GetCursor(uint64_t& x, uint64_t& y) {
    x = m_cursorX;
    y = m_cursorY;
}

uint64_t VGA::GetNumberOfRows() {
    return m_numberOfRows;
}

uint64_t VGA::GetNumberOfColumns() {
    return m_numberOfColumns;
}

void VGA::EnableDoubleBuffering(FrameBuffer* buffer) {
    m_backBuffer = buffer;
    memcpy(m_backBuffer->BaseAddress, m_frontBuffer->BaseAddress, m_frontBuffer->pitch * m_frontBuffer->height);
    m_doubleBufferingEnabled = true;
}

void VGA::DisableDoubleBuffering() {
    m_doubleBufferingEnabled = false;
    m_backBuffer = m_frontBuffer;
}

bool VGA::IsDoubleBufferingEnabled() {
    return m_doubleBufferingEnabled;
}

void VGA::SwapBuffers() {
    if (m_doubleBufferingEnabled)
        memcpy(m_frontBuffer->BaseAddress, m_backBuffer->BaseAddress, m_frontBuffer->pitch * m_frontBuffer->height);
}
