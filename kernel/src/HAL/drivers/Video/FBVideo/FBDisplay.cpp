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

#include "FBDisplay.hpp"
#include "FBVideoDevice.hpp"

#include "../Display.hpp"
#include "../VideoFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <util.h>

#include <frostyos/asm/video.h>

#include <Graphics/Framebuffer.hpp>

#include <Memory/VMM.hpp>

#include <SystemCalls/SystemCall.hpp>

#define VIDEO_DEFAULT_REFRESH_RATE 60

namespace Video {
    uint32_t PixelFormatFromFB(FrameBuffer* fb) {
#define PXF_CHECK(_bpp, rshift, rmask, gshift, gmask, bshift, bmask, format) if (fb->bpp == _bpp && fb->red_shift == rshift && fb->red_mask == rmask && fb->green_shift == gshift && fb->green_mask == gmask && fb->blue_shift == bshift && fb->blue_mask == bmask) return format

        PXF_CHECK(16, 11, 0x1F,  5, 0x3F,  0, 0x1F, FOSV_PIXFORM_RGB565);
        PXF_CHECK(32, 16, 0xFF,  8, 0xFF,  0, 0xFF, FOSV_PIXFORM_XRGB8888);
        PXF_CHECK(32, 16, 0xFF,  8, 0xFF,  0, 0xFF, FOSV_PIXFORM_ARGB8888);
        PXF_CHECK(32,  8, 0xFF, 16, 0xFF, 24, 0xFF, FOSV_PIXFORM_BGRX8888);
        PXF_CHECK(32,  8, 0xFF, 16, 0xFF, 24, 0xFF, FOSV_PIXFORM_BGRA8888);
        PXF_CHECK(32, 24, 0xFF, 16, 0xFF,  8, 0xFF, FOSV_PIXFORM_RGBX8888);
        PXF_CHECK(32, 24, 0xFF, 16, 0xFF,  8, 0xFF, FOSV_PIXFORM_RGBA8888);

#undef PXF_CHECK

        return FOSV_PIXFORM_UNKNOWN;
    }

    FBDisplay::FBDisplay(uint64_t id) : Display(id), m_framebuffer(nullptr), m_device(nullptr), m_memObj(nullptr) {

    }

    FBDisplay::FBDisplay(uint64_t id, FrameBuffer* framebuffer) : Display(id), m_framebuffer(framebuffer), m_device(nullptr), m_memObj(nullptr) {

    }

    FBDisplay::~FBDisplay() {

    }

    int FBDisplay::Init() {
        m_info.cap = FOSV_DCAP_LINEAR_FB;
        m_info.modeCount = 1;
        if (m_framebuffer != nullptr) {
            m_info.currentMode.width = m_framebuffer->width;
            m_info.currentMode.height = m_framebuffer->height;
            m_info.currentMode.pitch = m_framebuffer->pitch;
            m_info.currentMode.refreshRate = VIDEO_DEFAULT_REFRESH_RATE;
            m_info.currentMode.pixelClock = VIDEO_DEFAULT_REFRESH_RATE * m_framebuffer->width * m_framebuffer->height;
            m_info.currentMode.pixelFormat = PixelFormatFromFB(m_framebuffer);
            memcpy(&m_info.nativeMode, &m_info.currentMode, sizeof(FOSV_VideoMode));
            m_modes = &m_info.currentMode;
        }
        return ESUCCESS;
    }

    int FBDisplay::Open(int flags) {
        return ESUCCESS;
    }

    int FBDisplay::Close(int flags) {
        return ESUCCESS;
    }

    int FBDisplay::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead) {
        if (m_framebuffer == nullptr)
            return -ENODEV;

        if (out == nullptr)
            return -EFAULT;

        size_t bufferSize = m_framebuffer->pitch * m_framebuffer->height;

        if (size == 0 || offset >= bufferSize)
            return -EINVAL;

        size = MIN(size, bufferSize - offset);

        memcpy(out, m_framebuffer->BaseAddress, size);

        if (bytesRead != nullptr)
            *bytesRead = size;

        return ESUCCESS;
    }

    int FBDisplay::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten) {
        if (m_framebuffer == nullptr)
            return -ENODEV;

        if (in == nullptr)
            return -EFAULT;

        size_t bufferSize = m_framebuffer->pitch * m_framebuffer->height;

        if (size == 0 || offset >= bufferSize)
            return -EINVAL;

        size = MIN(size, bufferSize - offset);

        memcpy(m_framebuffer->BaseAddress, in, size);

        if (bytesWritten != nullptr)
            *bytesWritten = size;

        return ESUCCESS;
    }

    int FBDisplay::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj) {
        size_t bufferSize = m_framebuffer->pitch * m_framebuffer->height;
        if (offset + size > bufferSize || obj == nullptr || size == 0 || (offset & (PAGE_SIZE - 1)) > 0 || (size & (PAGE_SIZE - 1)) > 0)
            return -EINVAL;

        m_lock.Lock();
        if (m_memObj == nullptr) {
            m_memObj = (VMM::MemoryObject*)kcalloc_vmm(1, sizeof(VMM::MemoryObject));
            if (m_memObj == nullptr) {
                m_lock.Unlock();
                return -ENOMEM;
            }

            m_memObj->pager = m_device->GetPager();
            m_memObj->pagerData = this;
            m_memObj->size = bufferSize;
            m_memObj->refCount = 1;
        }
        m_lock.Unlock();

        *obj = m_memObj;
        return ESUCCESS;
    }

    int FBDisplay::Munmap() {
        return -ENOSYS;
    }

    int FBDisplay::Ioctl(size_t op, void* arg, int* result, Process* proc) {
        switch (op) {
        case FOSV_GET_DISPLAY_INFO:
            if (!UserWrite(arg, &m_info, sizeof(FOSV_DisplayInfo), proc))
                return EFAULT;
            return ESUCCESS;
        case FOSV_GET_DISPLAY_MODE: {
            FOSV_GetDisplayModeArg kArg;
            if (!UserRead(arg, &kArg, sizeof(FOSV_GetDisplayModeArg), proc))
                return EFAULT;
            if (kArg.mode > m_info.modeCount)
                return EINVAL;

            memcpy(&kArg.mode, &m_modes[kArg.mode], sizeof(FOSV_VideoMode));

            if (!UserWrite(arg, &kArg, sizeof(FOSV_GetDisplayModeArg), proc))
                return EFAULT;

            return ESUCCESS;
        }
        case FOSV_SET_DISPLAY_MODE:
            return ENOTSUP;
        default:
            return EINVAL;
        }
    }

    FrameBuffer* FBDisplay::GetFrameBuffer() {
        return m_framebuffer;
    }

    void FBDisplay::SetFrameBuffer(FrameBuffer* framebuffer) {
        m_framebuffer = framebuffer;
    }

    void FBDisplay::SetDevice(FBVideoDevice* device) {
        m_device = device;
    }

    FBVideoDevice* FBDisplay::GetDevice() {
        return m_device;
    }

    VMM::Protection FBDisplay::GetDefaultProt() {
        return VMM::Protection::READ_WRITE;
    }

}