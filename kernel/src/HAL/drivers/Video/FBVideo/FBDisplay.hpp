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

#ifndef _HAL_FBVIDEO_DISPLAY_HPP
#define _HAL_FBVIDEO_DISPLAY_HPP

#include <stddef.h>
#include <stdint.h>

#include <Graphics/Framebuffer.hpp>

#include <Memory/VMM.hpp>

#include "../Display.hpp"

namespace Video {
    class DisplayVNode;

    class FBVideoDevice;

    class FBDisplay : public Display {
    public:
        FBDisplay(uint64_t id);
        FBDisplay(uint64_t id, FrameBuffer* framebuffer);
        virtual ~FBDisplay();

        virtual int Init() override;

        virtual int Open(int flags) override;
        virtual int Close(int flags) override;
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead) override;
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten) override;
        virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj) override;
        virtual int Munmap() override;
        virtual int Ioctl(size_t op, void* arg, int* result, Process* proc) override;

        FrameBuffer* GetFrameBuffer();
        void SetFrameBuffer(FrameBuffer* framebuffer);

        void SetDevice(FBVideoDevice* device);
        FBVideoDevice* GetDevice();

        virtual VMM::Protection GetDefaultProt();

    private:
        FrameBuffer* m_framebuffer;
        FBVideoDevice* m_device;
        VMM::MemoryObject* m_memObj;
        Mutex m_lock;
    };
}


#endif /* _HAL_FBVIDEO_DISPLAY_HPP */