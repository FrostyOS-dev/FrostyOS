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

#include "NullDevice.hpp"

#include <errno.h>
#include <string.h>

NullDevice* g_NullDevice = nullptr;

NullDevice::NullDevice() : Device(), m_isOpen(false), m_info(nullptr) {

}

NullDevice::~NullDevice() {

}

int NullDevice::Init(DeviceManager::DeviceInfo* info) {
    m_info = info;
    
    if (info->manager == nullptr)
        return -EINVAL;

    return info->manager->CreateRootFile(info, "null");
}

int NullDevice::Open(int flags, const Credential& cred) {
    m_isOpen = true;
    return 0;
}

int NullDevice::Close(int flags, const Credential& cred) {
    m_isOpen = false;
    return 0;
}

int NullDevice::Read(void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential& cred) {
    if (!m_isOpen)
        return -EBADF;

    *bytesRead = 0;
    return 0;
}

int NullDevice::Write(const void* in, size_t size, int flags, uint64_t, size_t* bytesWritten, const Credential& cred) {
    if (!m_isOpen)
        return -EBADF;

    *bytesWritten = size;
    return 0;
}

int NullDevice::Mmap(uint64_t, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
    return -ENOSYS;
}

int NullDevice::Munmap() {
    return -ENOSYS;
}

int NullDevice::Ioctl(size_t op, void* arg, int* result, const Credential& cred) {
    return -ENOSYS;
}
