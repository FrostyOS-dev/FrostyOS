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

#include "File.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <Scheduling/Process.hpp>

int sys_open(const char* path, size_t pathLen, int flags, mode_t mode) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    char* realPath = new char[pathLen + 1];
    if (realPath == nullptr)
        return -ENOMEM;

    if (!UserRead(path, realPath, pathLen, proc)) {
        delete[] realPath;
        return -EFAULT;
    }

    realPath[pathLen] = 0;

    FS::VNode* cwd = proc->GetCWD();

    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_Open(realPath, &vnode, cwd, proc->GetCred());
    if (rc < 0) {
        if (rc == -ENOENT && (flags & O_CREAT) > 0) {
            if (realPath[pathLen - 1] == '/') {
                delete[] realPath;
                return -EISDIR;
            }
            char* parent = realPath;
            char* name = strrchr(realPath, '/');
            if (name == nullptr) {
                name = realPath;
                parent = (char*)"";
            } else {
                name[0] = 0;
                name++;
            }
            
            rc = FS::VFS_CreateFile(parent, name, cwd, proc->GetCred());
            if (rc < 0) {
                delete[] realPath;
                return rc;
            }

            if (name != realPath)
                name[-1] = '/';

            rc = FS::VFS_Open(realPath, &vnode, cwd, proc->GetCred());
            if (rc < 0) {
                delete[] realPath;
                return rc;
            }
        } else {
            delete[] realPath;
            return rc;
        }
    }

    vnode->Lock();
    FS::VType type = vnode->GetType();
    vnode->Unlock();
    if (type != FS::VType::REG && type != FS::VType::DIR) {
        FS::VFS_Close(vnode, proc->GetCred());
        return -ENOSYS;
    } else if (type != FS::VType::DIR && (flags & O_DIRECTORY) > 0) {
        FS::VFS_Close(vnode, proc->GetCred());
        return -ENOTDIR;
    }

    FileDescriptor* desc = new FileDescriptor(proc, type == FS::VType::REG ? FDType::File : FDType::Directory, vnode);
    if (desc == nullptr) {
        FS::VFS_Close(vnode, proc->GetCred());
        delete[] realPath;
        return -ENOMEM;
    }

    rc = desc->Open(flags);
    if (rc < 0) {
        delete desc;
        FS::VFS_Close(vnode, proc->GetCred());
        delete[] realPath;
        return rc;
    }

    fd_t fd = manager->Allocate(desc);
    if (fd < 0) {
        desc->Close();
        delete desc;
        FS::VFS_Close(vnode, proc->GetCred());
    }
    
    delete[] realPath;
    return fd;
}

int sys_close(int fd) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = nullptr;
    if (!manager->Free(fd, &desc))
        return -EBADF;

    if (!desc->isOpen())
        return -EBADF; // somehow wasn't open?
    
    desc->Close();

    if (desc->GetType() != FDType::File)
        return ESUCCESS; // nothing more to do

    FS::VNode* vnode = desc->GetVNode();
    if (vnode == nullptr)
        return -EBADF; // we don't try reopen the FD on errors during sys_close

    int rc = FS::VFS_Close(vnode, proc->GetCred());
    if (rc < 0)
        return rc;

    delete desc;

    return ESUCCESS;
}

ssize_t sys_read(int fd, void* buf, size_t count) {
    if (count == 0)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    VMM::VMM* vmm = proc->GetVMM();
    if (!vmm->ValidateWrite(buf, count))
        return -EFAULT;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen())
        return -EBADF;

    uint8_t* kBuf = new uint8_t[count];

    size_t realCount = 0;
    int rc = desc->Read(kBuf, count, &realCount);
    ssize_t result = rc < 0 ? rc : realCount;

    UserWrite(buf, kBuf, realCount, proc, false);
    
    delete[] kBuf;
    return result;
}

ssize_t sys_write(int fd, const void* buf, size_t count) {
    if (count == 0)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen())
        return -EBADF;

    uint8_t* kBuf = new uint8_t[count];

    if (!UserRead(buf, kBuf, count, proc)) {
        delete[] kBuf;
        return -EFAULT;
    }

    size_t realCount = 0;
    int rc = desc->Write(kBuf, count, &realCount);
    ssize_t result = rc < 0 ? rc : realCount;
    
    delete[] kBuf;
    return result;
}

off_t sys_seek(int fd, off_t offset, int whence) {
    if (whence < SEEK_SET || whence > SEEK_END)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen())
        return -EBADF;

    FDOffsetStart fdWhence;
    switch (whence) {
    case SEEK_SET:
        fdWhence = FDOffsetStart::START;
        break;
    case SEEK_CUR:
        fdWhence = FDOffsetStart::CURRENT;
        break;
    case SEEK_END:
        fdWhence = FDOffsetStart::END;
        break;
    default:
        return -EINVAL;
    }

    int64_t realOffset = 0;
    int rc = desc->Seek(offset, fdWhence, &realOffset);
    if (rc < 0)
        return rc;

    return realOffset;
}

int sys_isatty(int fd) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen())
        return -EBADF;

    switch (desc->GetType()) {
    case FDType::TTY:
        return 0;
    default:
        return -ENOTTY;
    }
}

int sys_getdents(int fd, void* buf, size_t maxRead, size_t* bytesRead) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen())
        return -EBADF;

    size_t count = maxRead / sizeof(FS::Dentry);
    if (count == 0)
        return -EINVAL;

    FS::Dentry* kBuf = new FS::Dentry[count];
    if (kBuf == nullptr)
        return -ENOMEM;

    size_t realCount = 0;
    int rc = desc->GetDents(kBuf, count, &realCount);
    if (rc < 0) {
        delete[] kBuf;
        return rc;
    }

    if (!UserWrite(buf, kBuf, sizeof(FS::Dentry) * count, proc))
        rc = -EFAULT;

    realCount *= sizeof(FS::Dentry);
    if (!UserWrite(bytesRead, &realCount, sizeof(size_t), proc))
        rc = -EFAULT;

    delete[] kBuf;
    return rc;
}

int sys_getcwd(char* buf, size_t size) {
    if (buf == nullptr || size == 0)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (!vmm->ValidateWrite(buf, size))
        return -EFAULT;

    FS::VNode* vnode = proc->GetCWD();

    char* kBuf = new char[size];
    if (kBuf == nullptr)
        return -ENOMEM;

    int rc = FS::VFS_BuildPath(vnode, kBuf, size, proc->GetCred());
    if (rc < 0) {
        delete[] kBuf;
        return rc;
    }

    bool ret = UserWrite(buf, kBuf, size, proc, false);

    delete[] kBuf;

    return ret ? ESUCCESS : -EFAULT;
}
