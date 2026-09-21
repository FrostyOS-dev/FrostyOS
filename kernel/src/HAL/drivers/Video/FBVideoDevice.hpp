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

#ifndef _VGA_HPP
#define _VGA_HPP

#include <stdint.h>

#include <Graphics/Colour.hpp>
#include <Graphics/Framebuffer.hpp>

#include "VideoDevice.hpp"

class FBVideoDevice : public VideoDevice {
public:
    FBVideoDevice();
    FBVideoDevice(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour);
    virtual ~FBVideoDevice() override;

    virtual int Init() override;
    int Init(FrameBuffer* framebuffer, Colour backgroundColour, Colour foregroundColour);

    virtual void ClearScreen() override;
    virtual void ClearScreen(Colour colour) override;

    void PlotPixel(uint64_t x, uint64_t y, Colour colour);
    void DrawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);
    void DrawFilledRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Colour colour);

    void SetFrameBuffer(FrameBuffer* framebuffer);

    virtual void SetBackgroundColour(Colour& colour) override;
    virtual void SetForegroundColour(Colour& colour) override;

    FrameBuffer* GetFrameBuffer() const;

    virtual Colour GetBackgroundColour() const override;
    virtual Colour GetForegroundColour() const override;

    virtual void PrintChar(char c) override;
    virtual void PrintString(const char* str) override;
    virtual void PrintString(const char* str, uint64_t length) override;

    virtual void Backspace() override;
    virtual void NewLine() override;

    virtual void Scroll(uint64_t n) override;
    
    virtual void SetCursor(uint64_t x, uint64_t y) override;
    virtual void GetCursor(uint64_t& x, uint64_t& y) override;

    virtual uint64_t GetNumberOfRows() override;
    virtual uint64_t GetNumberOfColumns() override;

    virtual uint64_t GetWidth() override;
    virtual uint64_t GetHeight() override;

    void EnableDoubleBuffering(FrameBuffer* buffer);
    void DisableDoubleBuffering();
    bool IsDoubleBufferingEnabled();

    void SwapBuffers();

private:
    FrameBuffer* m_frontBuffer;
    FrameBuffer* m_backBuffer;
    Colour m_backgroundColour;
    Colour m_foregroundColour;

    uint64_t m_cursorX;
    uint64_t m_cursorY;
    uint64_t m_numberOfRows;
    uint64_t m_numberOfColumns;

    bool m_doubleBufferingEnabled;
};

#endif /* _VGA_HPP */