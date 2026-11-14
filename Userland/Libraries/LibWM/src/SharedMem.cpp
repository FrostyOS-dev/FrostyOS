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

#include <LibWM/SharedMem.hpp>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

namespace LibWM {
    SharedMemory SharedMemory::Create(size_t size) {
        if (size == 0 || size > MaxPoolSize) {
            errno = EINVAL;
            return {};
        }

        int fd = -1;
        char name[64];
        for (int attempt = 0; attempt < 32 && fd < 0; attempt++) {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            snprintf(name, sizeof(name), "/libwm-%d-%lx-%d", (int)getpid(), (unsigned long)ts.tv_nsec, attempt);
            fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (fd < 0 && errno != EEXIST) {
                int e = errno;
                fprintf(stderr, "shm_open failed: %s\n", strerror(e));
                errno = e;
                return {};
            }
        }

        if (fd < 0)
            return {};

        shm_unlink(name); // unlink the file, fd is the only handle
        fcntl(fd, F_SETFD, FD_CLOEXEC);

        if (ftruncate(fd, (off_t)size) < 0) {
            int e = errno;
            fprintf(stderr, "ftruncate failed: %s\n", strerror(e));
            close(fd);
            errno = e;
            return {};
        }

        void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            int e = errno;
            fprintf(stderr, "mmap failed: %s\n", strerror(e));
            close(fd);
            errno = e;
            return {};
        }

        SharedMemory m;
        m.m_data = static_cast<uint8_t*>(p);
        m.m_size = size;
        m.m_fd = fd;
        return m;
    }

    SharedMemory SharedMemory::Import(int fd, size_t size) {
        if (fd < 0) {
            errno = EBADF;
            return {};
        }

        struct stat st;
        if (size == 0 || size > MaxPoolSize || fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || (uint64_t)st.st_size < size) {
            close(fd);
            errno = EINVAL;
            return {};
        }

        void* p = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            int e = errno;
            close(fd);
            errno = e;
            return {};
        }

        SharedMemory m;
        m.m_data = static_cast<uint8_t*>(p);
        m.m_size = size;
        m.m_fd = fd;
        return m;
    }

    void SharedMemory::Reset() {
        if (m_data != nullptr)
            munmap(m_data, m_size);
        if (m_fd >= 0)
            close(m_fd);
        m_data = nullptr;
        m_size = 0;
        m_fd = -1;
    }

    void SharedMemory::Take(SharedMemory& o) {
        m_data = o.m_data;
        m_size = o.m_size;
        m_fd = o.m_fd;

        o.m_data = nullptr;
        o.m_size = 0;
        o.m_fd = -1;
    }

}