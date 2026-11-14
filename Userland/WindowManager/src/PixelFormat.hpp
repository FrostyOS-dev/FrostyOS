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

#ifndef _WM_PIXEL_FORMAT_HPP
#define _WM_PIXEL_FORMAT_HPP

#include <stddef.h>
#include <stdint.h>

namespace WM {
    // The compositor always renders in XRGB8888. All other formats are a display format that is converted to.
    enum class PixelFormat {
        Unknown,
        RGB565,
        XRGB8888,
        ARGB8888,
        BGRX8888,
        BGRA8888,
        RGBX8888,
        RGBA8888
    };

    size_t BytesPerPixel(PixelFormat f);
    const char* PixelFormatName(PixelFormat f);
    PixelFormat ParsePixelFormat(const char* name);

    typedef void (*ConvertRowFn)(void* dst, const uint32_t* src, size_t count);

    ConvertRowFn GetRowConverter(PixelFormat f);
}

#endif /* _WM_PIXEL_FORMAT_HPP */