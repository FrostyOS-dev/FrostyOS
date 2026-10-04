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

#ifndef _HAL_VIDEO_DISPLAY_HPP
#define _HAL_VIDEO_DISPLAY_HPP

#include <stddef.h>
#include <stdint.h>

#include <frostyos/asm/video.h>

#include <Memory/VMM.hpp>

namespace FS {
    class VFS;
}

class Process;

namespace Video {
    class DisplayVNode;

    class Display {
    public:
        Display(uint64_t id);
        virtual ~Display();

        virtual int Init();

        virtual int Open(int flags);
        virtual int Close(int flags);
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead);
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten);
        virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj);
        virtual int Munmap();
        virtual int Ioctl(size_t op, void* arg, int* result, Process* proc);

        uint64_t GetID() const;
        void SetID(uint64_t);

        DisplayVNode* CreateVNode(FS::VFS* vfs);
        DisplayVNode* GetVNode();

    protected:
        FOSV_DisplayInfo m_info;
        FOSV_VideoMode* m_modes;

    private:
        uint64_t m_id;
        DisplayVNode* m_vnode;
    };
}

#endif /* _HAL_VIDEO_DISPLAY_HPP */