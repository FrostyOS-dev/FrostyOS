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

#include "FileDescriptor.hpp"
#include "VFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <Scheduling/Mutex.hpp>

#include <SystemCalls/File.hpp>

#include <tty/TTY.hpp>
#include <tty/TTYBackend.hpp>

FileDescriptor::FileDescriptor() : m_proc(nullptr), m_type(FDType::Invalid), m_offset(0), m_open(false), m_append(false), m_vnode(nullptr), m_tty(nullptr), m_ttyStream(TTYBackendStream::INVALID) {

}

FileDescriptor::FileDescriptor(Process* proc, FDType type, FS::VNode* vnode) : m_proc(proc), m_type(type), m_offset(0), m_open(false), m_append(false), m_vnode(vnode), m_tty(nullptr), m_ttyStream(TTYBackendStream::INVALID) {

}

FileDescriptor::FileDescriptor(Process* proc, FDType type, TTY* tty, TTYBackendStream stream) : m_proc(proc), m_type(type), m_offset(0), m_open(false), m_append(false), m_vnode(nullptr), m_tty(tty), m_ttyStream(stream) {

}

FileDescriptor::~FileDescriptor() {

}

void FileDescriptor::Init(Process* proc, FDType type, FS::VNode* vnode) {

}

void FileDescriptor::Init(Process* proc, FDType type, TTY* tty, TTYBackendStream stream) {

}


int FileDescriptor::Open(int flags) {
    m_mutex.Lock();
    if (m_proc == nullptr) {
        m_mutex.Unlock();
        return -EBADF;
    }

    switch (m_type) {
    case FDType::File:
        if ((flags & O_APPEND) > 0) {
            if (m_vnode == nullptr) {
                m_mutex.Unlock();
                return -EBADF;
            }
            m_append = true;
        }
        break;
    case FDType::TTY:
        if ((flags & O_APPEND) == 0)
            break;
        m_mutex.Unlock();
        return -EBADF;
    case FDType::Invalid:
        m_mutex.Unlock();
        return -EBADF;
    }

    m_open = true;
    m_offset = 0;
    m_mutex.Unlock();
    return ESUCCESS;
}

void FileDescriptor::Close() {
    m_mutex.Lock();
    m_offset = 0;
    m_append = false;
    m_open = false;
    m_mutex.Unlock();
}

bool FileDescriptor::isOpen() const {
    return m_open;
}

int FileDescriptor::Read(void* buf, size_t count, size_t* realCount) {
    if (buf == nullptr || count == 0 || realCount == nullptr)
        return -EINVAL;

    m_mutex.Lock();

    if (!m_open) {
        m_mutex.Unlock();
        return -EBADF;
    }

    int rc = -ENOSYS;

    switch(m_type) {
    case FDType::File: {
        if (m_vnode == nullptr) {
            m_mutex.Unlock();
            return -EBADF;
        }
        size_t bytesRead = 0;
        rc = m_vnode->Read(buf, count, 0, m_offset, &bytesRead, m_proc->GetCred());
        m_offset += bytesRead;
        *realCount = bytesRead;
        break;
    }
    case FDType::TTY: {
        if (m_tty == nullptr || m_ttyStream == TTYBackendStream::INVALID) {
            m_mutex.Unlock();
            return -EBADF;
        }
        // Ignore offset
        m_tty->ReadString(static_cast<char*>(buf), count, m_ttyStream);
        *realCount = count;
        rc = ESUCCESS;
        break;
    }
    }

    m_mutex.Unlock();
    return rc;
}

int FileDescriptor::Write(const void* buf, size_t count, size_t* realCount) {
    if (buf == nullptr || count == 0 || realCount == nullptr)
        return -EINVAL;

    m_mutex.Lock();

    if (!m_open) {
        m_mutex.Unlock();
        return -EBADF;
    }

    int rc = -ENOSYS;

    switch(m_type) {
    case FDType::File: {
        if (m_vnode == nullptr) {
            m_mutex.Unlock();
            return -EBADF;
        }
        size_t bytesWritten = 0;
        size_t offset = m_offset;
        if (m_append) {
            FS::VAttr attr;
            rc = m_vnode->GetAttr(&attr);
            if (rc < 0)
                break;
            offset = attr.size;
        }
        rc = m_vnode->Write(buf, count, 0, offset, &bytesWritten, m_proc->GetCred());
        if (!m_append)
            m_offset += bytesWritten;
        *realCount = bytesWritten;
        break;
    }
    case FDType::TTY: {
        if (m_tty == nullptr || m_ttyStream == TTYBackendStream::INVALID) {
            m_mutex.Unlock();
            return -EBADF;
        }
        // Ignore offset
        m_tty->WriteString(static_cast<const char*>(buf), count, m_ttyStream, true);
        *realCount = count;
        rc = ESUCCESS;
        break;
    }
    }

    m_mutex.Unlock();
    return rc;
}

int FileDescriptor::Seek(int64_t offset, FDOffsetStart whence, int64_t* realOffset) {
    if (realOffset == nullptr)
        return -EINVAL;

    m_mutex.Lock();

    if (!m_open) {
        m_mutex.Unlock();
        return -EBADF;
    }

    int rc = -ENOSYS;

    switch (m_type) {
    case FDType::File: {
        if (m_vnode == nullptr) {
            rc = -EBADF;
            break;
        }
        FS::VAttr attr;
        rc = m_vnode->GetAttr(&attr);
        if (rc < 0)
            break;
        size_t fileSize = attr.size;

        switch (whence) {
        case FDOffsetStart::START:
            if (offset < 0 || (uint64_t)offset > fileSize) {
                rc = -EINVAL;
                break;
            }
            m_offset = offset;
            *realOffset = offset;
            rc = ESUCCESS;
            break;
        case FDOffsetStart::END:
            if (offset > 0 || (uint64_t)(-offset) > fileSize) {
                rc = -EINVAL;
                break;
            }
            m_offset = fileSize + offset;
            *realOffset = m_offset;
            rc = ESUCCESS;
            break;
        case FDOffsetStart::CURRENT: {
            if ((offset < 0 && (uint64_t)(-offset) > m_offset) || (offset > 0 && (m_offset + offset) > fileSize)) {
                rc = -EINVAL;
                break;
            }
            m_offset += offset;
            *realOffset = m_offset;
            rc = ESUCCESS;
            break;
        }
        }
    }
    }

    m_mutex.Unlock();
    return rc;
}

FDType FileDescriptor::GetType() const {
    return m_type;
}

FS::VNode* FileDescriptor::GetVNode() {
    return m_vnode;
}

