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

#include "FBVideoDevice.hpp"

#include <string.h>
#include <util.h>

#include <Graphics/Colour.hpp>
#include <Graphics/Framebuffer.hpp>
#include <Graphics/VGAFont.hpp>

FBVideoDevice::FBVideoDevice() : VideoDevice(), m_frontBuffer(nullptr), m_backBuffer(nullptr), m_backgroundColour(0, 0, 0), m_foregroundColour(255, 255, 255), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_doubleBufferingEnabled(false) {

}

FBVideoDevice::FBVideoDevice(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour) : VideoDevice(), m_frontBuffer(framebuffer), m_backBuffer(framebuffer), m_backgroundColour(backgroundColour), m_foregroundColour(foregroundColour), m_cursorX(0), m_cursorY(0), m_numberOfRows(0), m_numberOfColumns(0), m_doubleBufferingEnabled(false) {
    m_numberOfRows = m_backBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_backBuffer->width / CHAR_WIDTH;
}

FBVideoDevice::~FBVideoDevice() {

}

int FBVideoDevice::Init() {
    m_numberOfRows = m_backBuffer->height / CHAR_HEIGHT;
    m_numberOfColumns = m_backBuffer->width / CHAR_WIDTH;

    m_cursorX = 0;
    m_cursorY = 0;

    ClearScreen(m_backgroundColour);
    return 0;
}

int FBVideoDevice::Init(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour) {
    m_backBuffer = framebuffer;
    m_frontBuffer = framebuffer;
    m_backgroundColour = backgroundColour;
    m_foregroundColour = foregroundColour;

    return Init();
}

void FBVideoDevice::ClearScreen() {
    ClearFrameBuffer(m_backBuffer, m_backgroundColour);
}

void FBVideoDevice::ClearScreen(Colour colour) {
    ClearFrameBuffer(m_backBuffer, colour);
}

void FBVideoDevice::PlotPixel(uint64_t x, uint64_t y, Colour colour) {
    WriteToFrameBuffer(m_backBuffer, x, y, colour);
}

void FBVideoDevice::DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
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

void FBVideoDevice::DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour) {
    for (uint64_t i = x; i < x + width; i++) {
        for (uint64_t j = y; j < y + height; j++) {
            PlotPixel(i, j, colour);
        }
    }
}

void FBVideoDevice::SetFrameBuffer(FrameBuffer* framebuffer) {
    m_backBuffer = framebuffer;
}

void FBVideoDevice::SetBackgroundColour(Colour& colour) {
    m_backgroundColour = colour;
}

void FBVideoDevice::SetForegroundColour(Colour& colour) {
    m_foregroundColour = colour;
}

FrameBuffer* FBVideoDevice::GetFrameBuffer() const {
    return m_backBuffer;
}

Colour FBVideoDevice::GetBackgroundColour() const {
    return m_backgroundColour;
}

Colour FBVideoDevice::GetForegroundColour() const {
    return m_foregroundColour;
}

void FBVideoDevice::PrintChar(char c) {
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
        if (c >= ' ' && c < 0x7F) {
            WriteCharToFrameBuffer(m_backBuffer, m_cursorX, m_cursorY, m_foregroundColour, m_backgroundColour, c);
            m_cursorX += CHAR_WIDTH;

            if (m_cursorX >= (m_numberOfColumns * CHAR_WIDTH))
                NewLine();
        }
        break;
    }
}

void FBVideoDevice::PrintString(const char* str) {
    for (uint64_t i = 0; str[i] != '\0'; i++) {
        PrintChar(str[i]);
    }
}

void FBVideoDevice::PrintString(const char* str, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        PrintChar(str[i]);
    }
}

void FBVideoDevice::Backspace() {
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

void FBVideoDevice::NewLine() {
    m_cursorX = 0;
    m_cursorY += CHAR_HEIGHT;

    if (m_cursorY >= (m_numberOfRows * CHAR_HEIGHT))
        Scroll(1);
}

void FBVideoDevice::Scroll(uint64_t n) {
    memcpy(m_backBuffer->BaseAddress, (void*)((uint64_t)m_backBuffer->BaseAddress + n * m_backBuffer->pitch * CHAR_HEIGHT), m_backBuffer->pitch * (m_backBuffer->height - n * CHAR_HEIGHT));
    DrawFilledRectangle(0, m_backBuffer->height - n * CHAR_HEIGHT, m_backBuffer->width, n * CHAR_HEIGHT, m_backgroundColour);
    m_cursorY -= n * CHAR_HEIGHT;
}

void FBVideoDevice::SetCursor(uint64_t x, uint64_t y) {
    m_cursorX = x;
    m_cursorY = y;
}

void FBVideoDevice::GetCursor(uint64_t& x, uint64_t& y) {
    x = m_cursorX;
    y = m_cursorY;
}

uint64_t FBVideoDevice::GetNumberOfRows() {
    return m_numberOfRows;
}

uint64_t FBVideoDevice::GetNumberOfColumns() {
    return m_numberOfColumns;
}

void FBVideoDevice::EnableDoubleBuffering(FrameBuffer* buffer) {
    m_backBuffer = buffer;
    memcpy(m_backBuffer->BaseAddress, m_frontBuffer->BaseAddress, m_frontBuffer->pitch * m_frontBuffer->height);
    m_doubleBufferingEnabled = true;
}

void FBVideoDevice::DisableDoubleBuffering() {
    m_doubleBufferingEnabled = false;
    m_backBuffer = m_frontBuffer;
}

bool FBVideoDevice::IsDoubleBufferingEnabled() {
    return m_doubleBufferingEnabled;
}

void FBVideoDevice::SwapBuffers() {
    if (m_doubleBufferingEnabled)
        memcpy(m_frontBuffer->BaseAddress, m_backBuffer->BaseAddress, m_frontBuffer->pitch * m_frontBuffer->height);
}
