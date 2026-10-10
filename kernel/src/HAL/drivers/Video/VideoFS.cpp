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

#include <fs/DevTempFS/DevTempFS.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#define DISPLAY_OP(op, ...) if (m_display == nullptr) return -ENODEV; return m_display->op(__VA_ARGS__)

namespace Video {
    DisplayVNode::DisplayVNode(FS::VFS* vfs, Display* display) : FS::DevTempFSVNode(vfs, nullptr), m_display(display) {

    }

    DisplayVNode::~DisplayVNode() {

    }

    int DisplayVNode::Open(int flags, const Credential& cred) {
        DISPLAY_OP(Open, flags);
    }

    int DisplayVNode::Close(int flags, const Credential& cred) {
        DISPLAY_OP(Close, flags);
    }

    int DisplayVNode::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) {
        DISPLAY_OP(Read, out, size, flags, offset, bytesRead);
    }

    int DisplayVNode::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) {
        DISPLAY_OP(Write, in, size, flags, offset, bytesWritten);
    }

    int DisplayVNode::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
        DISPLAY_OP(Mmap, offset, size, obj);
    }

    int DisplayVNode::Munmap() {
        DISPLAY_OP(Munmap);
    }

    int DisplayVNode::Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) {
        DISPLAY_OP(Ioctl, op, arg, result, proc);
    }

}