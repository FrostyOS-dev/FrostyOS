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

#include "PixelFormat.hpp"

#include <string.h>
#include <strings.h>

namespace WM {
    size_t BytesPerPixel(PixelFormat f) {
        switch (f) {
        case PixelFormat::RGB565:
            return 2;
        case PixelFormat::Unknown:
            return 0;
        default:
            return 4;
        }
    }

    static const struct {
        PixelFormat format;
        const char* name;
    } g_formats[] = {
        {PixelFormat::RGB565, "rgb565"},
        {PixelFormat::XRGB8888, "xrgb8888"},
        {PixelFormat::ARGB8888, "argb8888"},
        {PixelFormat::BGRX8888, "bgrx8888"},
        {PixelFormat::BGRA8888, "bgra8888"},
        {PixelFormat::RGBX8888, "rgbx8888"},
        {PixelFormat::RGBA8888, "rgba8888"}
    };

    const char* PixelFormatName(PixelFormat f) {
        for (auto& e : g_formats) {
            if (e.format == f)
                return e.name;
        }
        return "unknown";
    }

    PixelFormat ParsePixelFormat(const char* name) {
        for (auto& e : g_formats) {
            if (strcasecmp(e.name, name) == 0)
                return e.format;
        }
        return PixelFormat::Unknown;
    }

    static void RowCopy(void* dst, const uint32_t* src, size_t count) {
        memcpy(dst, src, count * 4);
    }

    static void RowBGRA(void* dst, const uint32_t* src, size_t count) {
        uint32_t* out = static_cast<uint32_t*>(dst);
        for (size_t i = 0; i < count; i++)
            out[i] = __builtin_bswap32(src[i] | 0xFF000000u);
    }

    static void RowRGBA(void* dst, const uint32_t* src, size_t count) {
        uint32_t* out = static_cast<uint32_t*>(dst);
        for (size_t i = 0; i < count; i++) {
            uint32_t v = src[i] | 0xFF000000u;
            out[i] = (v << 8) | (v >> 24);
        }
    }

    static void RowRGB565(void* dst, const uint32_t* src, size_t count) {
        uint16_t* out = static_cast<uint16_t*>(dst);
        for (size_t i = 0; i < count; i++) {
            uint32_t v = src[i];
            out[i] = (uint16_t)(((v >> 8) & 0xF800) | ((v >> 5) & 0x07E0) | ((v >> 3) & 0x001F));
        }
    }

    ConvertRowFn GetRowConverter(PixelFormat f) {
        switch (f) {
        case PixelFormat::RGB565:
            return RowRGB565;
        case PixelFormat::XRGB8888:
        case PixelFormat::ARGB8888:
            return RowCopy;
        case PixelFormat::BGRX8888:
        case PixelFormat::BGRA8888:
            return RowBGRA;
        case PixelFormat::RGBX8888:
        case PixelFormat::RGBA8888:
            return RowRGBA;
        default:
            return nullptr;
        }
    }
}