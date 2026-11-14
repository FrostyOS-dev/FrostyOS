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

#include "FrostyOSVideoBackend.hpp"
#include "PixelFormat.hpp"

#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utility>

#include <asm/video.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

namespace WM {
    static PixelFormat ConvertFormat(uint32_t f) {
        switch (f) {
        case FOSV_PIXFORM_RGB565:
            return PixelFormat::RGB565;
        case FOSV_PIXFORM_XRGB8888:
            return PixelFormat::XRGB8888;
        case FOSV_PIXFORM_ARGB8888:
            return PixelFormat::ARGB8888;
        case FOSV_PIXFORM_BGRX8888:
            return PixelFormat::BGRX8888;
        case FOSV_PIXFORM_BGRA8888:
            return PixelFormat::BGRA8888;
        case FOSV_PIXFORM_RGBX8888:
            return PixelFormat::RGBX8888;
        case FOSV_PIXFORM_RGBA8888:
            return PixelFormat::RGBA8888;
        default:
            return PixelFormat::Unknown;
        }
    }

    static VideoMode ConvertMode(const FOSV_VideoMode& m) {
        VideoMode v;
        v.width = m.width;
        v.height = m.height;
        v.pitch = m.pitch;
        v.refreshRate = m.refreshRate;
        v.format = ConvertFormat(m.pixelFormat);
        return v;
    }

    class FrostyOSDisplay final : public LinearFBDisplay {
    public:
        static std::unique_ptr<FrostyOSDisplay> Open(const std::string& path, const std::string& name) {
            std::unique_ptr<FrostyOSDisplay> d(new FrostyOSDisplay());
            if (!d->Init(path, name))
                return nullptr;
            return d;
        }

        ~FrostyOSDisplay() override {
            DetachFramebuffer();
            Unmap();
            if (m_fd >= 0)
                close(m_fd);
        }

        const std::string& Name() const override { return m_name; }
        bool CanSetMode() const override { return m_canSetMode; }
        const std::vector<VideoMode>& Modes() const override { return m_modes; }
        bool HasAccurateTiming() const override { return m_accurateTiming; }

        bool SetMode(size_t index) override {
            if (!m_canSetMode || index >= m_modes.size())
                return false;

            // TODO: actually set the mode
            return false; // return false for now

            DetachFramebuffer();
            Unmap();
            FOSV_DisplayInfo info = {};
            if (ioctl(m_fd, FOSV_GET_DISPLAY_INFO, &info) < 0)
                return false;
            return Map(info.currentMode);
        }

    private:
        FrostyOSDisplay() = default;

        bool Init(const std::string& path, const std::string& name) {
            m_name = name;
            m_fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
            if (m_fd < 0)
                return false;

            FOSV_DisplayInfo info = {};
            if (ioctl(m_fd, FOSV_GET_DISPLAY_INFO, &info) < 0)
                return false;

            if ((info.cap & FOSV_DCAP_LINEAR_FB) == 0) {
                fprintf(stderr, "WindowManager: %s has no linear framebuffer, skipping\n", name.c_str());
                return false;
            }
            m_accurateTiming = (info.cap & FOSV_DCAP_ACCURATE_TIME) != 0;
            m_canSetMode = (info.cap & FOSV_DCAP_MODESET) != 0;

            if (m_canSetMode) {
                for (uint32_t i = 0; i < info.modeCount; i++) {
                    FOSV_GetDisplayModeArg arg = {};
                    arg.mode = i;
                    if (ioctl(m_fd, FOSV_GET_DISPLAY_MODE, &arg) < 0)
                        break;
                    m_rawModes.push_back(arg.info);
                    m_modes.push_back(ConvertMode(arg.info));
                }
                if (m_modes.size() != info.modeCount)
                    m_canSetMode = false; // failed to enumerate modes, don't allow setting
            }

            return Map(info.currentMode);
        }

        bool Map(const FOSV_VideoMode& raw) {
            VideoMode mode = ConvertMode(raw);
            size_t size = (size_t)mode.pitch * mode.height;
            if (size == 0)
                return false;
            void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
            if (p == MAP_FAILED)
                return false;
            if (!AttachFramebuffer(p, size, mode)) {
                fprintf(stderr, "WindowManager: %s: unusable mode %ux%u pitch %u format %s\n", m_name.c_str(), mode.width, mode.height, mode.pitch, PixelFormatName(mode.format));
                munmap(p, size);
                return false;
            }
            m_map = p;
            m_mapSize = size;
            return true;
        }

        void Unmap() {
            if (m_map != nullptr)
                munmap(m_map, m_mapSize);
            m_map = nullptr;
            m_mapSize = 0;
        }
        
        int m_fd = -1;
        std::string m_name;
        void* m_map = nullptr;
        size_t m_mapSize = 0;
        bool m_canSetMode = false;
        bool m_accurateTiming = false;
        std::vector<FOSV_VideoMode> m_rawModes;
        std::vector<VideoMode> m_modes;
    };

    static bool ParseIndex(const char* name, const char* prefix, int& out) {
        size_t n = strlen(prefix);
        if (strncmp(name, prefix, n) != 0 || name[n] == 0)
            return false;
        char* end;
        long v = strtol(name + n, &end, 10);
        if (*end != 0 || v < 0)
            return false;
        out = (int)v;
        return true;
    }

    static std::vector<int> ListIndices(const char* dir, const char* prefix) {
        std::vector<int> out;
        DIR* d = opendir(dir);
        if (d == nullptr)
            return out;
        while (dirent* e = readdir(d)) {
            int idx;
            if (ParseIndex(e->d_name, prefix, idx))
                out.push_back(idx);
        }
        closedir(d);
        std::sort(out.begin(), out.end());
        return out;
    }

    bool FrostyOSVideoBackend::Initialise() {
        for (int gpu : ListIndices("/dev", "gpu")) {
            std::string gpuDir = "/dev/gpu" + std::to_string(gpu);
            for (int disp : ListIndices(gpuDir.c_str(), "display")) {
                std::string path = gpuDir + "/display" + std::to_string(disp);
                std::string name = "gpu" + std::to_string(gpu) + "/display" + std::to_string(disp);
                auto d = FrostyOSDisplay::Open(path, name);
                if (d == nullptr) {
                    fprintf(stderr, "WindowManager: Could not use %s\n", path.c_str());
                    continue;
                }
                m_displays.push_back(std::move(d));
            }
        }
        return !m_displays.empty();
    }
}