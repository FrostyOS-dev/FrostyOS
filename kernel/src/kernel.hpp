/*
Copyright (©) 2024-2025  Frosty515

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

#ifndef _KERNEL_HPP
#define _KERNEL_HPP

#include <stdint.h>

#include <Graphics/Framebuffer.hpp>

#include <Memory/MemoryMap.hpp>

#include <HAL/HAL.hpp>

struct KernelParams {
    uint64_t HHDMStart;
    FrameBuffer framebuffer;
    MemoryMapEntry** MemoryMap;
    uint64_t MemoryMapEntryCount;
    void* RSDP;
    uint64_t kernelPhysical;
    uint64_t kernelVirtual;
    PagingMode pagingMode;
};

extern KernelParams g_kernelParams;

void StartKernel();
void Kernel_Stage2(void*);

#endif /* _KERNEL_HPP */