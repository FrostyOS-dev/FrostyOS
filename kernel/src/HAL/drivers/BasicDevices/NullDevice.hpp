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

#ifndef _HAL_NULL_DEVICE_HPP
#define _HAL_NULL_DEVICE_HPP

#include <stddef.h>
#include <stdint.h>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#include "../Device.hpp"
#include "../DeviceManager.hpp"

class NullDevice : public Device {
public:
    NullDevice();
    virtual ~NullDevice();

    virtual int Init(DeviceManager::DeviceInfo* info) override;

    virtual int Open(int flags, const Credential& cred) override;
    virtual int Close(int flags, const Credential& cred) override;
    virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) override;
    virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) override;
    virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) override;
    virtual int Munmap() override;
    virtual int Ioctl(size_t op, void* arg, int* result, const Credential& cred) override;

private:
    bool m_isOpen;
    DeviceManager::DeviceInfo* m_info;
};

extern NullDevice* g_NullDevice;

#endif /* _HAL_NULL_DEVICE_HPP */