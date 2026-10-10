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

#ifndef _HAL_VIDEO_FS_HPP
#define _HAL_VIDEO_FS_HPP

#include <stdint.h>

#include <fs/VFS.hpp>

#include <fs/DevTempFS/DevTempFS.hpp>

namespace Video {

    class Display;

    class DisplayVNode : public FS::DevTempFSVNode {
    public:
        DisplayVNode(FS::VFS* vfs, Display* display);
        virtual ~DisplayVNode();

        virtual int Open(int flags, const Credential& cred) override;
        virtual int Close(int flags, const Credential& cred) override;
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) override;
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) override;
        virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) override;
        virtual int Munmap() override;
        virtual int Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) override;

    private:
        Display* m_display;
    };

}

#endif /* _HAL_VIDEO_FS_HPP */