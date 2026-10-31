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

#include <Scheduling/Event.hpp>
#include <Scheduling/Mutex.hpp>
#include <Scheduling/Process.hpp>

enum class FDType {
    File,
    Directory,
    Pipe,
    Invalid
};

enum class FDOffsetStart {
    START,
    END,
    CURRENT
};

namespace FS {
    class VNode;
    struct Dentry;
}

class Pipe;

class Process;

class FileDescriptor {
public:
    FileDescriptor();
    FileDescriptor(Process* proc, FDType type, FS::VNode* vnode);
    FileDescriptor(Process* proc, Pipe* pipe, bool writeEnd);
    ~FileDescriptor();

    void Init(Process* proc, FDType type, FS::VNode* vnode);

    int Open(int flags);
    bool isOpen() const;

    int Read(void* buf, size_t count, size_t* realCount);
    int Write(const void* buf, size_t count, size_t* realCount);

    int Seek(int64_t offset, FDOffsetStart whence, int64_t* realOffset);

    int GetDents(FS::Dentry* buf, size_t count, size_t* realCount);

    int Ioctl(uint64_t op, void* arg, int* result, Process* currentProc);

    EventWaitQueue* GetPollQueue(uint32_t events);
    uint32_t PollReady(uint32_t events);

    int GetFlags();
    int SetFlags(int flags);

    FDType GetType() const;
    FS::VNode* GetVNode();

    void Ref();
    void Unref();

private:
    bool IsDevice();

    Process* m_proc; // only used by Open()
    FDType m_type;

    size_t m_offset;
    bool m_open;

    FS::VNode* m_vnode;

    Mutex m_mutex;
    uint64_t m_refCount;

    Credential m_cred;
    int m_flags;
    bool m_canRead;
    bool m_canWrite;

    Pipe* m_pipe;
    bool m_pipeWriteEnd;
};

#endif /* _FILE_DESCRIPTOR_HPP */