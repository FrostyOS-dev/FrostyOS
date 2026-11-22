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

#include "FrostyOSInputBackend.hpp"

#include "../InputDevice.hpp"

#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <stdio.h>
#include <unistd.h>

#include <asm/input.h>

#include <sys/ioctl.h>

#include <Keys.hpp>

namespace WM {
    static bool IntIoctl(int fd, unsigned long request, int value) {
        return ioctl(fd, request, &value) == 0;
    }

    static EventType ConvertEventType(uint32_t t) {
        switch (t) {
        case FOSI_ET_KEY:
            return EventType::Key;
        case FOSI_ET_REL:
            return EventType::Rel;
        case FOSI_ET_SYN:
            return EventType::Syn;
        default:
            return EventType::Other;
        }
    }

    class FrostyOSInputDevice final : public InputDevice {
    public:
        static std::unique_ptr<FrostyOSInputDevice> Open(const std::string& path) {
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                return nullptr;

            FOSI_DeviceInfo info = {};
            if (ioctl(fd, FOSI_GET_DEVICE_INFO, &info) < 0 || (info.type != FOSI_DT_KEYBOARD && info.type != FOSI_DT_MOUSE)) {
                close(fd);
                return nullptr;
            }

            if (!IntIoctl(fd, FOSI_SET_CLOCK, FOSI_CLOCK_MONOTONIC)) {
                close(fd);
                return nullptr;
            }

            DeviceType type = info.type == FOSI_DT_KEYBOARD ? DeviceType::Keyboard : DeviceType::Mouse;
            return std::unique_ptr<FrostyOSInputDevice>(new FrostyOSInputDevice(fd, path, type));
        }

        ~FrostyOSInputDevice() override {
            if (m_fd >= 0)
                close(m_fd);
        }

        const std::string& Path() const override {
            return m_path;
        }

        DeviceType Type() const override {
            return m_type;
        }

        int FD() const override {
            return m_fd;
        }

        ClaimResult Claim() override {
            if (m_claimed)
                return ClaimResult::Ok;
            if (IntIoctl(m_fd, FOSI_SET_CLAIM, 1)) {
                m_claimed = true;
                return ClaimResult::Ok;
            }
            return errno == EBUSY ? ClaimResult::Busy : ClaimResult::Error;
        }

        void Release() override {
            if (m_claimed) {
                IntIoctl(m_fd, FOSI_SET_CLAIM, 0);
                m_claimed = false;
            }
        }

        bool GetKeyState(KeyBits& out) override {
            FOSI_KeyState st = {};
            if (ioctl(m_fd, FOSI_GET_KEY_STATE, &st) < 0)
                return false;
            out.reset();
            for (uint32_t k = 0; k < KeyCount; k++) {
                if ((st.bits[k / 8] & (1u << (k % k))) != 0)
                    out.set(k);
            }
            return true;
        }

        bool Read(std::vector<RawEvent>& out) override {
            FOSI_Event buf[64];
            while (true) {
                ssize_t n = read(m_fd, buf, sizeof(buf));
                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return true;
                    return false;
                }
                if (n == 0)
                    return false;

                size_t count = (size_t)n / sizeof(FOSI_Event);
                for (size_t i = 0; i < count; i++)
                    out.push_back({(uint64_t)buf[i].tv_sec * UINT64_C(1'000'000'000) + (uint64_t)buf[i].tv_nsec, ConvertEventType(buf[i].type), buf[i].code, buf[i].value});
                if ((size_t)n < sizeof(buf))
                    return true;
            }
        }

    private:
        FrostyOSInputDevice(int fd, std::string path, DeviceType type) : m_fd(fd), m_path(std::move(path)), m_type(type) {
        }

        int m_fd;
        std::string m_path;
        DeviceType m_type;
        bool m_claimed = false;
    };

    bool FrostyOSInputBackend::Initialise() {
        DIR* d = opendir(m_dir.c_str());
        if (d == nullptr)
            return false;
        std::vector<std::string> names;
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] != '.')
                names.push_back(e->d_name);
        }
        closedir(d);
        std::sort(names.begin(), names.end());

        for (const std::string& n : names) {
            auto dev= FrostyOSInputDevice::Open(m_dir + "/" + n);
            if (dev != nullptr)
                m_devices.push_back(std::move(dev));
        }
        return !m_devices.empty();
    }
}