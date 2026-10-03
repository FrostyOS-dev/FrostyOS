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

#include "ZeroDevice.hpp"

#include <errno.h>
#include <string.h>

ZeroDevice* g_ZeroDevice = nullptr;

ZeroDevice::ZeroDevice() : Device(), m_isOpen(false), m_info(nullptr) {

}

ZeroDevice::~ZeroDevice() {

}

int ZeroDevice::Init(DeviceManager::DeviceInfo* info) {
    m_info = info;
    
    if (info->manager == nullptr)
        return -EINVAL;

    return info->manager->CreateRootFile(info, "zero");
}

int ZeroDevice::Open(int flags, const Credential& cred) {
    m_isOpen = true;
    return 0;
}

int ZeroDevice::Close(int flags, const Credential& cred) {
    m_isOpen = false;
    return 0;
}

int ZeroDevice::Read(void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential& cred) {
    if (!m_isOpen)
        return -EBADF;

    if (out == nullptr || size == 0)
        return out == nullptr ? -EFAULT : -EINVAL;

    memset(out, 0, size);
    *bytesRead = size;

    return 0;
}

int ZeroDevice::Write(const void* in, size_t size, int flags, uint64_t, size_t* bytesWritten, const Credential& cred) {
    if (!m_isOpen)
        return -EBADF;

    *bytesWritten = size;
    return 0;
}

int ZeroDevice::Mmap(uint64_t, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
    return -ENOSYS;
}

int ZeroDevice::Munmap() {
    return -ENOSYS;
}

int ZeroDevice::Ioctl(size_t op, void* arg, int* result, const Credential& cred) {
    return -ENOSYS;
}
