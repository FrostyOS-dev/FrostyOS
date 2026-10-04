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

#ifndef _HAL_DEVICE_HPP
#define _HAL_DEVICE_HPP

#include <stddef.h>
#include <stdint.h>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#include "DeviceManager.hpp"

class Device {
public:
    Device() {}
    virtual ~Device() {}

    virtual int Init(DeviceManager::DeviceInfo* info) = 0;

    virtual int Open(int flags, const Credential& cred) = 0;
    virtual int Close(int flags, const Credential& cred) = 0;
    virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) = 0;
    virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) = 0;
    virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) = 0;
    virtual int Munmap() = 0;
    virtual int Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) = 0;
};

#endif /* _HAL_DEVICE_HPP */