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

#ifndef _WM_VIDEO_BACKEND_HPP
#define _WM_VIDEO_BACKEND_HPP

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "PixelFormat.hpp"
#include "Rect.hpp"

namespace WM {
    struct VideoMode {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t pitch = 0; // bytes per row
        uint32_t refreshRate = 0; // 0 = unknown
        PixelFormat format = PixelFormat::Unknown;
    };

    class VideoDisplay {
    public:
        virtual ~VideoDisplay() = default;

        virtual const std::string& Name() const = 0;
        virtual const VideoMode& CurrentMode() const = 0;

        virtual void Present(const uint32_t* src, size_t srcStride, const Rect& area) = 0;

        virtual bool CanSetMode() const { return false; }
        virtual const std::vector<VideoMode>& Modes() const {
            static const std::vector<VideoMode> none;
            return none;
        }
        virtual bool SetMode(size_t index) {
            (void)index;
            return false;
        }
        virtual bool HasAccurateTiming() const { return false; }
    };

    class VideoBackend {
    public:
        virtual ~VideoBackend() = default;

        virtual bool Initialise() = 0;

        size_t DisplayCount() const { return m_displays.size(); }
        VideoDisplay& GetDisplay(size_t i) { return *m_displays[i]; }

    protected:
        std::vector<std::unique_ptr<VideoDisplay>> m_displays;
    };

    class LinearFBDisplay : public VideoDisplay {
    public:
        const VideoMode& CurrentMode() const override { return m_mode; }
        void Present(const uint32_t* src, size_t srcStride, const Rect& area) override;

    protected:
        bool AttachFramebuffer(void* base, size_t size, const VideoMode& mode);
        void DetachFramebuffer();

    private:
        uint8_t* m_fb = nullptr;
        size_t m_fbSize = 0;
        VideoMode m_mode;
        ConvertRowFn m_convert = nullptr;
        size_t m_bpp = 0;
    };
}

#endif /* _WM_VIDEO_BACKEND_HPP */