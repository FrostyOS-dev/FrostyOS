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

#ifndef _VIDEO_DEVICE_HPP
#define _VIDEO_DEVICE_HPP

#include <stdint.h>

#include <Graphics/Colour.hpp>

class VideoDevice {
public:
    VideoDevice() {}
    virtual ~VideoDevice() {}

    virtual int Init() = 0;

    virtual void ClearScreen() = 0;
    virtual void ClearScreen(Colour colour) = 0;

    virtual void SetBackgroundColour(Colour& colour) = 0;
    virtual void SetForegroundColour(Colour& colour) = 0;

    virtual Colour GetBackgroundColour() const = 0;
    virtual Colour GetForegroundColour() const = 0;

    virtual void PrintChar(char c) = 0;
    virtual void PrintString(const char* str) = 0;
    virtual void PrintString(const char* str, uint64_t length) = 0;

    virtual void Backspace() = 0;
    virtual void NewLine() = 0;

    virtual void Scroll(uint64_t n) = 0;
    
    virtual void SetCursor(uint64_t x, uint64_t y) = 0;
    virtual void GetCursor(uint64_t& x, uint64_t& y) = 0;

    virtual uint64_t GetNumberOfRows() = 0;
    virtual uint64_t GetNumberOfColumns() = 0;

    virtual uint64_t GetWidth() = 0;
    virtual uint64_t GetHeight() = 0;
};

#endif /* _VIDEO_DEVICE_HPP */