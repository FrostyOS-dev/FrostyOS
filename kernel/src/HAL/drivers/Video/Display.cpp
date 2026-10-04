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

#include "Display.hpp"
#include "VideoFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <Memory/VMM.hpp>

namespace Video {
    Display::Display(uint64_t id) : m_id(id) {

    }

    Display::~Display() {

    }

    int Display::Init() {
        return 0;
    }

    int Display::Open(int flags) {
        return -ENOTSUP;
    }

    int Display::Close(int flags) {
        return -ENOTSUP;
    }

    int Display::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead) {
        return -ENOTSUP;
    }

    int Display::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten) {
        return -ENOTSUP;
    }

    int Display::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj) {
        return -ENOTSUP;
    }

    int Display::Munmap() {
        return -ENOTSUP;
    }

    int Display::Ioctl(size_t op, void* arg, int* result, Process* proc) {
        return -ENOTSUP;
    }

    uint64_t Display::GetID() const {
        return m_id;
    }

    void Display::SetID(uint64_t id) {
        m_id = id;
    }

    DisplayVNode* Display::CreateVNode(FS::VFS* vfs) {
        m_vnode = new DisplayVNode(vfs, this);
        return m_vnode;
    }

    DisplayVNode* Display::GetVNode() {
        return m_vnode;
    }
    
}