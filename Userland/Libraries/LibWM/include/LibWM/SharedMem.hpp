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

#ifndef _FOS_LIBWM_SHAREDMEM_HPP
#define _FOS_LIBWM_SHAREDMEM_HPP

#include <stddef.h>
#include <stdint.h>

namespace LibWM {
    class SharedMemory {
    public:
        SharedMemory() = default;
        ~SharedMemory() { Reset(); }
        SharedMemory(const SharedMemory&) = delete;
        SharedMemory& operator=(const SharedMemory&) = delete;
        SharedMemory(SharedMemory&& o) noexcept { Take(o); }
        SharedMemory& operator=(SharedMemory&& o) noexcept {
            if (this != &o) {
                Reset();
                Take(o);
            }
            return *this;
        }

        static SharedMemory Create(size_t size);

        static SharedMemory Import(int fd, size_t size);

        bool Valid() const {
            return m_data != nullptr;
        }
        uint8_t* Data() const {
            return m_data;
        }
        size_t Size() const {
            return m_size;
        }
        int FD() const {
            return m_fd;
        }

        void Reset();

    private:
        void Take(SharedMemory& o);

        uint8_t* m_data = nullptr;
        size_t m_size = 0;
        int m_fd = -1;
    };

    constexpr size_t MaxPoolSize = 256u * 1024 * 1024; // 256MiB

    inline bool BufferInBounds(size_t poolSize, uint32_t offset, uint32_t height, uint32_t stride) {
        uint64_t end = (uint64_t)stride * height;
        end += offset;
        return stride != 0 && height != 0 && end <= poolSize;
    }
}

#endif /* _FOS_LIBWM_SHAREDMEM_HPP */