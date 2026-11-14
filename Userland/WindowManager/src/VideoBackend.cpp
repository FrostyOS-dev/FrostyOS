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
#include "VideoBackend.hpp"

namespace WM {
    void LinearFBDisplay::Present(const uint32_t* src, size_t srcStride, const Rect& area) {
        if (m_fb == nullptr)
            return;
        Rect r = area.Interset({0, 0, (int)m_mode.width, (int)m_mode.height});
        if (r.Empty())
            return;

        size_t count = (size_t)r.Width();
        for (int y = r.y0; y < r.y1; y++) {
            uint8_t* dst = m_fb + (size_t)y * m_mode.pitch + (size_t)r.x0 * m_bpp;
            m_convert(dst, src + (size_t)y * srcStride + r.x0, count);
        }
    }

    bool LinearFBDisplay::AttachFramebuffer(void* base, size_t size, const VideoMode& mode) {
        DetachFramebuffer();

        size_t bpp = BytesPerPixel(mode.format);
        ConvertRowFn convert = GetRowConverter(mode.format);
        if (bpp == 0 || convert == nullptr)
            return false;
        if (mode.width == 0 || mode.height == 0)
            return false;

        if (mode.pitch < (uint64_t)mode.width * bpp || mode.pitch % bpp != 0)
            return false;
        if ((uint64_t)mode.pitch * mode.height > size)
            return false;
        if (reinterpret_cast<uintptr_t>(base) % bpp != 0)
            return false;

        m_fb = static_cast<uint8_t*>(base);
        m_fbSize = size;
        m_mode = mode;
        m_convert = convert;
        m_bpp = bpp;
        return true;
    }

    void LinearFBDisplay::DetachFramebuffer() {
        m_fb = nullptr;
        m_fbSize = 0;
        m_convert = nullptr;
        m_bpp = 0;
    }

}