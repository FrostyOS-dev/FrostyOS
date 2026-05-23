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

#ifndef _FILE_DESCRIPTOR_HPP
#define _FILE_DESCRIPTOR_HPP

#include <stddef.h>
#include <stdint.h>

#include <Scheduling/Mutex.hpp>

#include <tty/TTY.hpp>
#include <tty/TTYBackend.hpp>

enum class FDType {
    File,
    TTY,
    Invalid
};

enum class FDOffsetStart {
    START,
    END,
    CURRENT
};

enum FDOpenFlags {
    FD_FLAG_O_APPEND = 0x1
};

namespace FS {
    class VNode;
}

class Process;

class FileDescriptor {
public:
    FileDescriptor();
    FileDescriptor(Process* proc, FDType type, FS::VNode* vnode);
    FileDescriptor(Process* proc, FDType type, TTY* tty, TTYBackendStream stream);
    ~FileDescriptor();

    void Init(Process* proc, FDType type, FS::VNode* vnode);
    void Init(Process* proc, FDType type, TTY* tty, TTYBackendStream stream);

    int Open(int flags);
    int Close();

    int Read(void* buf, size_t count, size_t* realCount);
    int Write(const void* buf, size_t count, size_t* realCount);

    int Seek(int64_t offset, FDOffsetStart whence, int64_t* realOffset);

private:
    Process* m_proc;
    FDType m_type;

    size_t m_offset;
    bool m_open;
    bool m_append;

    FS::VNode* m_vnode;

    TTY* m_tty;
    TTYBackendStream m_ttyStream;

    Mutex m_mutex;
};

#endif /* _FILE_DESCRIPTOR_HPP */