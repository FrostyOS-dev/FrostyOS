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
#include "Signal.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/Pipe.hpp>
#include <fs/VFS.hpp>

#include <HAL/Time.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Event.hpp>
#include <Scheduling/Process.hpp>

static void SplitPath(char* path, const char** parent, const char** name) {
    char* slash = strrchr(path, '/');
    if (slash == nullptr) {
        *parent = "";
        *name = path;
    } else if (slash == path) {
        *parent = "/";
        *name = path + 1;
    } else {
        *slash = 0;
        *parent = path;
        *name = slash + 1;
    }
}

int sys_openat(int dirFD, const char* path, size_t pathLen, int flags, mode_t mode) {
    if (pathLen == 0)
        return -ENOENT;

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

    FS::VNode* dirVNode = nullptr;
    if (dirFD == AT_FDCWD)
        dirVNode = proc->GetCWDRef();
    else {
        FileDescriptor* desc = manager->Get(dirFD);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
            if (desc != nullptr)
                desc->Unref();
            delete[] realPath;
            return -EBADF;
        }

        dirVNode = desc->GetVNode();
        FS::RefVNode(dirVNode);
        desc->Unref();
    }

    if (dirVNode == nullptr) {
        delete[] realPath;
        return -EINVAL;
    }

    bool created = false;
    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_Open(realPath, flags, &vnode, dirVNode, proc->GetCred());
    if (rc < 0) {
        if (rc == -ENOENT && (flags & O_CREAT) > 0) {
            if (realPath[pathLen - 1] == '/') {
                delete[] realPath;
                FS::UnrefVNode(dirVNode);
                return -EISDIR;
            }
            const char* parent;
            const char* name;
            SplitPath(realPath, &parent, &name);
            
            bool open = false;
            mode_t createMode = mode & ~proc->GetUMask() & S_PERMMASK;
            rc = FS::VFS_Create(parent, name, FS::VType::REG, createMode, dirVNode, proc->GetCred(), nullptr, &vnode);
            if ((flags & O_EXCL) == 0 && rc == -EEXIST) {
                if (parent == realPath)
                    ((char*)name)[-1] = '/';
                rc = FS::VFS_Open(realPath, flags, &vnode, dirVNode, proc->GetCred());
                open = true;
            } else
                created = (rc == 0);

            FS::UnrefVNode(dirVNode);

            if (rc < 0 || vnode == nullptr) {
                delete[] realPath;
                return rc < 0 ? rc : -ENOTSUP;
            }

            if (!open) {
                vnode->Lock();
                rc = vnode->Open(flags, proc->GetCred());
                vnode->Unlock();
                if (rc < 0) {
                    delete[] realPath;
                    return rc;
                }

                FS::RefVNode(vnode);
            }
        } else {
            FS::UnrefVNode(dirVNode);
            delete[] realPath;
            return rc;
        }
    } else {
        FS::UnrefVNode(dirVNode);
        if (rc == 0 && (flags & O_CREAT) > 0 && (flags & O_EXCL) > 0) {
            FS::VFS_Close(vnode, proc->GetCred());
            delete[] realPath;
            return -EEXIST;
        }
    }

    delete[] realPath;

    FS::VType type = vnode->GetType();
    if (type != FS::VType::REG && type != FS::VType::DIR && type != FS::VType::CHR && type != FS::VType::BLK) {
        FS::VFS_Close(vnode, proc->GetCred());
        return -ENOTSUP;
    } else if (type != FS::VType::DIR && (flags & O_DIRECTORY) > 0) {
        FS::VFS_Close(vnode, proc->GetCred());
        return -ENOTDIR;
    }

    if ((flags & O_TRUNC) > 0 && type == FS::VType::REG) {
        rc = FS::VFS_Truncate(vnode, 0, proc->GetCred(), !created);
        if (rc < 0) {
            FS::VFS_Close(vnode, proc->GetCred());
            return rc;
        }
    }

    FileDescriptor* desc = new FileDescriptor(proc, type != FS::VType::DIR ? FDType::File : FDType::Directory, vnode);
    if (desc == nullptr) {
        FS::VFS_Close(vnode, proc->GetCred());
        return -ENOMEM;
    }

    rc = desc->Open(flags);
    if (rc < 0) {
        delete desc;
        return rc;
    }

    fd_t fd = manager->Allocate(desc, (flags & O_CLOEXEC) > 0 ? FD_CLOEXEC : 0);
    if (fd < 0)
        delete desc;
    
    return fd;
}

int sys_close(int fd) {
    if (fd < 0)
        return -EBADF;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    if (!manager->Free(fd))
        return -EBADF;
    return ESUCCESS;
}

ssize_t sys_read(int fd, void* buf, size_t count) {
    if (count == 0)
        return 0;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    VMM::VMM* vmm = proc->GetVMM();
    if (!vmm->ValidateWrite(buf, count))
        return -EFAULT;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    uint8_t* kBuf = new uint8_t[count];

    size_t realCount = 0;
    int rc = desc->Read(kBuf, count, &realCount);
    ssize_t result = rc < 0 ? rc : realCount;

    desc->Unref();

    UserWrite(buf, kBuf, realCount, proc);
    
    delete[] kBuf;
    return result;
}

ssize_t sys_write(int fd, const void* buf, size_t count) {
    if (count == 0)
        return 0;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    uint8_t* kBuf = new uint8_t[count];

    if (!UserRead(buf, kBuf, count, proc)) {
        desc->Unref();
        delete[] kBuf;
        return -EFAULT;
    }

    size_t realCount = 0;
    int rc = desc->Write(kBuf, count, &realCount);
    ssize_t result = rc < 0 ? rc : realCount;

    desc->Unref();
    
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
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

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
        desc->Unref();
        return -EINVAL;
    }

    int64_t realOffset = 0;
    int rc = desc->Seek(offset, fdWhence, &realOffset);
    desc->Unref();
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
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    FS::VNode* vnode = desc->GetVNode();
    if (vnode == nullptr) {
        desc->Unref();
        return -ENOTTY;
    }

    int rc = vnode->Isatty();
    desc->Unref();
    return rc;
}

int sys_getdents(int fd, void* buf, size_t maxRead, size_t* bytesRead) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    size_t count = maxRead / sizeof(FS::Dentry);
    if (count == 0) {
        desc->Unref();
        return -EINVAL;
    }
    
    FS::Dentry* kBuf = new FS::Dentry[count];
    if (kBuf == nullptr) {
        desc->Unref();
        return -ENOMEM;
    }

    size_t realCount = 0;
    int rc = desc->GetDents(kBuf, count, &realCount);
    desc->Unref();
    if (rc < 0) {
        delete[] kBuf;
        return rc;
    }

    if (!UserWrite(buf, kBuf, sizeof(FS::Dentry) * realCount, proc))
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

    FS::VNode* vnode = proc->GetCWDRef();

    char* kBuf = new char[size];
    if (kBuf == nullptr) {
        FS::UnrefVNode(vnode);
        return -ENOMEM;
    }

    int rc = FS::VFS_BuildPath(vnode, kBuf, size, proc->GetCred());
    FS::UnrefVNode(vnode);
    if (rc < 0) {
        delete[] kBuf;
        return rc;
    }

    bool ret = UserWrite(buf, kBuf, size, proc);

    delete[] kBuf;

    return ret ? ESUCCESS : -EFAULT;
}

int sys_symlinkat(int dirFD, const char* target, size_t targetLen, const char* linkPath, size_t linkLen) {
    if (target == nullptr || targetLen == 0 || linkPath == nullptr || linkLen == 0)
        return -EINVAL;

    if (targetLen > PATH_MAX || linkLen > PATH_MAX)
        return -ENAMETOOLONG;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    char* kTarget = new char[targetLen + 1];
    char* kLinkPath = new char[linkLen + 1];
    if (kTarget == nullptr || kLinkPath == nullptr) {
        if (kTarget != nullptr)
            delete[] kTarget;
        if (kLinkPath != nullptr)
            delete[] kLinkPath;
        return -ENOMEM;
    }

    if (!UserRead(target, kTarget, targetLen, proc) || !UserRead(linkPath, kLinkPath, linkLen, proc)) {
        delete[] kTarget;
        delete[] kLinkPath;
        return -EFAULT;
    }

    kTarget[targetLen] = 0;
    kLinkPath[linkLen] = 0;

    FS::VNode* dirVNode = nullptr;
    if (dirFD == AT_FDCWD)
        dirVNode = proc->GetCWDRef();
    else {
        FileDescriptor* desc = manager->Get(dirFD);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
            if (desc != nullptr)
                desc->Unref();
            delete[] kTarget;
            delete[] kLinkPath;
            return -EBADF;
        }

        dirVNode = desc->GetVNode();
        FS::RefVNode(dirVNode);
        desc->Unref();
    }

    if (dirVNode == nullptr) {
        FS::UnrefVNode(dirVNode);
        delete[] kTarget;
        delete[] kLinkPath;
        return -EINVAL;
    }

    // Need to split the path
    if (kLinkPath[linkLen - 1] == '/') {
        FS::UnrefVNode(dirVNode);
        delete[] kTarget;
        delete[] kLinkPath;
        return -EISDIR;
    }
    const char* parent;
    const char* name;
    SplitPath(kLinkPath, &parent, &name);

    int rc = FS::VFS_Create(parent, name, FS::VType::LNK, DEFAULT_SYMLINK_MODE, dirVNode, proc->GetCred(), nullptr, nullptr, kTarget);

    FS::UnrefVNode(dirVNode);
    delete[] kTarget;
    delete[] kLinkPath;

    if (rc < 0)
        return rc;

    return rc;
}

int sys_chdir(const char* path, size_t pathLen) {
    if (pathLen == 0)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();

    char* kPath = new char[pathLen + 1];
    if (kPath == nullptr)
        return -ENOMEM;

    if (!UserRead(path, kPath, pathLen, proc)) {
        delete[] kPath;
        return -EFAULT;
    }

    kPath[pathLen] = 0;

    FS::VNode* vnode = nullptr;
    FS::VFS* vfs = nullptr;
    int rc = FS::VFS_LookupPath(kPath, &vnode, &vfs, proc->GetCWD(), proc->GetCred());
    delete[] kPath;
    
    if (rc == 0) {
        if (vnode->GetType() != FS::VType::DIR) {
            FS::UnrefVNode(vnode);
            return -ENOTDIR;
        }
        rc = vnode->Access(ACCESS_X, proc->GetCred());
        if (rc < 0) {
            FS::UnrefVNode(vnode);
            return rc;
        }
        FS::UnrefVNode(proc->GetCWD());
        proc->SetCWD(vnode);
    }

    return rc;
}

int sys_fchdir(int fd) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    if (desc->GetType() != FDType::Directory) {
        desc->Unref();
        return -ENOTDIR;
    }

    FS::VNode* vnode = desc->GetVNode();
    if (vnode == nullptr) {
        desc->Unref();
        return -EBADF;
    }

    if (int rc = vnode->Access(ACCESS_X, proc->GetCred()); rc < 0) {
        desc->Unref();
        return rc;
    }

    FS::RefVNode(vnode);
    desc->Unref();
    FS::UnrefVNode(proc->GetCWD());
    proc->SetCWD(vnode);

    return 0;
}

int sys_ioctl(int fd, size_t op, void* arg, int* result) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr || vmm == nullptr)
        return ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return EBADF;
    }

    if (!vmm->ValidateWrite(result, sizeof(int))) {
        desc->Unref();
        return EFAULT;
    }

    int res = 0;
    int rc = desc->Ioctl(op, arg, &res, proc);
    desc->Unref();
    if (rc < 0)
        rc = -rc; // ioctl always returns errors as a positive value

    if (!UserWrite(result, &res, sizeof(int), proc))
        return EFAULT; // unlikely to fail

    return rc;
}

static int ResolveVNodeAt(Process* proc, int fd, const char* path, size_t pathLen, int flags, FS::VNode** out) {
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    if ((flags & AT_EMPTY_PATH) > 0 && pathLen == 0) {
        FS::VNode* v = nullptr;
        if (fd == AT_FDCWD)
            v = proc->GetCWDRef();
        else {
            FileDescriptor* desc = manager->Get(fd);
            if (desc == nullptr || !desc->isOpen()) {
                if (desc != nullptr)
                    desc->Unref();
                return -EBADF;
            }
            v = desc->GetVNode();
            if (v == nullptr) { // TTY or Pipe
                desc->Unref();
                return -ENOTSUP;
            }
            FS::RefVNode(v);
            desc->Unref();
        }
        if (v == nullptr)
            return -EBADF;
        *out = v;
        return ESUCCESS;
    }

    if (path == nullptr || pathLen == 0)
        return -ENOENT;
    if (pathLen > PATH_MAX)
        return -ENAMETOOLONG;

    char* kPath = new char[pathLen + 1];
    if (kPath == nullptr)
        return -ENOMEM;

    if (!UserRead(path, kPath, pathLen, proc)) {
        delete[] kPath;
        return -EFAULT;
    }
    kPath[pathLen] = 0;

    FS::VNode* base = nullptr;
    if (kPath[0] != '/') {
        if (fd == AT_FDCWD)
            base = proc->GetCWDRef();
        else {
            FileDescriptor* desc = manager->Get(fd);
            if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
                bool notDir = desc != nullptr && desc->isOpen();
                if (desc != nullptr)
                    desc->Unref();
                delete[] kPath;
                return notDir ? -ENOTDIR : -EBADF;
            }
            base = desc->GetVNode();
            FS::RefVNode(base);
            desc->Unref();
        }
        if (base == nullptr) {
            delete[] kPath;
            return -EBADF;
        }
    }

    FS::VNode* found = nullptr;
    FS::VFS* fs = nullptr;
    int rc = FS::VFS_LookupPath(kPath, &found, &fs, base, proc->GetCred(), (flags & AT_SYMLINK_NOFOLLOW) == 0);
    FS::UnrefVNode(base);
    delete[] kPath;
    if (rc < 0)
        return rc;

    *out = found;
    return ESUCCESS;
}

int sys_fstatat(int fd, const char* path, size_t pathLen, Stat* stat, int flags) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, fd, path, pathLen, flags, &vnode);
    if (rc == -ENOTSUP && fd != AT_FDCWD) { // TTY
        FileDescriptor* desc = manager->Get(fd);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Pipe) {
            if (desc != nullptr)
                desc->Unref();
            return -EBADF;
        }

        desc->Unref();
        Stat buf;
        memset(&buf, 0, sizeof(Stat));
        buf.mode = 0600 | ((FS::VFS_GetPosixType(FS::VType::FIFO) & 0xF) << 12);
        buf.blksize = PIPE_BUF;
        return UserWrite(stat, &buf, sizeof(Stat), proc) ? ESUCCESS : -EFAULT;
    }
    if (rc < 0)
        return rc;

    Stat buf;
    memset(&buf, 0, sizeof(Stat));

    dev_t devNum = vnode->GetVFS()->GetDevNum();

    FS::VAttr attr;
    rc = vnode->GetAttr(&attr);
    FS::UnrefVNode(vnode);
    if (rc < 0)
        return rc;

    buf.dev = devNum;
    buf.rdev = 0;
    buf.ino = attr.inode;
    buf.nlink = attr.nlinks;
    buf.mode = attr.mode | ((FS::VFS_GetPosixType(attr.type) & 0xF) << 12);
    buf.uid = attr.uid;
    buf.gid = attr.gid;
    buf.size = attr.size;
    buf.blksize = attr.fsBlockSize;
    buf.blocks = attr.blocks;
    buf.atime = attr.atime;
    buf.mtime = attr.mtime;
    buf.ctime = attr.ctime;

    if (!UserWrite(stat, &buf, sizeof(Stat), proc))
        return -EFAULT;

    return ESUCCESS;
}

int sys_mkdirat(int dirFD, const char* path, size_t pathLen, mode_t mode) {
    if (path == nullptr || pathLen == 0)
        return path == nullptr ? -EFAULT : -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    const Credential& cred = proc->GetCred();

    char* kPath = new char[pathLen + 1];
    if (kPath == nullptr)
        return -ENOMEM;

    if (!UserRead(path, kPath, pathLen, proc)) {
        delete[] kPath;
        return -EFAULT;
    }

    kPath[pathLen] = '\0';

    FS::VNode* dirVNode = nullptr;
    if (dirFD == AT_FDCWD)
        dirVNode = proc->GetCWDRef();
    else {
        FileDescriptor* desc = manager->Get(dirFD);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
            if (desc != nullptr)
                desc->Unref();
            delete[] kPath;
            return -EBADF;
        }

        dirVNode = desc->GetVNode();
        FS::RefVNode(dirVNode);
        desc->Unref();
    }

    if (dirVNode == nullptr) {
        delete[] kPath;
        return -EINVAL;
    }

    const char* parent;
    const char* name;
    SplitPath(kPath, &parent, &name);

    mode_t createMode = mode & ~proc->GetUMask() & (0777 | S_ISVTX);
    int rc = FS::VFS_Create(parent, name, FS::VType::DIR, createMode, dirVNode, cred, nullptr, nullptr);
    FS::UnrefVNode(dirVNode);
    delete[] kPath;
    return rc;
}

struct PollEntry {
    EventWaitNode node;
    FileDescriptor* desc;
    size_t index;
    uint32_t mask;
};

static int TimespecToMS(const timespec* ts, uint64_t* out) {
    if (ts == nullptr) {
        *out = UINT64_MAX;
        return 0;
    }

    if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1'000'000'000)
        return -EINVAL;

    uint64_t ms = (uint64_t)ts->tv_sec * 1000 + (ts->tv_nsec + 999'999) / 1'000'000;
    *out = (ms >= UINT64_MAX / 2000) ? UINT64_MAX : ms;
    return 0;
}

int doPoll(PollFD* fds, nfds_t nfds, const timespec* ts, Thread* thread) {
    if (nfds > MAX_FD)
        return -EINVAL;

    Process* proc = thread->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOTSUP;

    uint64_t timeoutMS;
    if (int rc = TimespecToMS(ts, &timeoutMS); rc < 0)
        return rc;

    PollFD* kFDs = nullptr;
    PollEntry* entries = nullptr;
    EventWaitNode** nodePtrs = nullptr;

    if (nfds > 0) {
        kFDs = new PollFD[nfds];
        entries = new PollEntry[nfds]();
        nodePtrs = new EventWaitNode*[nfds];
        if (kFDs == nullptr || entries == nullptr || nodePtrs == nullptr) {
            if (kFDs != nullptr)
                delete[] kFDs;
            if (entries != nullptr)
                delete[] entries;
            if (nodePtrs != nullptr)
                delete[] nodePtrs;
            return -ENOMEM;
        }
        if (!UserRead(fds, kFDs, nfds * sizeof(PollFD), proc)) {
            delete[] kFDs;
            delete[] entries;
            delete[] nodePtrs;
            return -EFAULT;
        }
    }

    size_t n = 0;
    int nval = 0;

    for (size_t i = 0; i < nfds; i++) {
        kFDs[i].revents = 0;

        if (kFDs[i].fd < 0)
            continue; // ignore negative FDs

        FileDescriptor* d = manager->Get(kFDs[i].fd);
        if (d == nullptr) {
            kFDs[i].revents = POLLNVAL;
            nval++;
            continue;
        }

        uint32_t events = (unsigned short)kFDs[i].events;
        PollEntry& e = entries[n];
        e.desc = d;
        e.index = i;
        e.mask = events | POLLERR | POLLHUP | POLLNVAL;
        e.node.requestedEvents = e.mask;
        e.node.queue = d->GetPollQueue(events);
        nodePtrs[n++] = &e.node;
    }

    Event::RegisterWaitNodes(nodePtrs, n);

    uint64_t deadline = (timeoutMS == UINT64_MAX) ? UINT64_MAX : HAL_GetNSTicks() / 1'000'000 + timeoutMS;
    int result = 0;

    while (true) {
        for (size_t j = 0; j < n; j++)
            entries[j].node.triggeredEvents = 0;

        int ready = nval;
        for (size_t j = 0; j < n; j++) {
            PollFD& p = kFDs[entries[j].index];
            uint32_t r = entries[j].desc->PollReady((unsigned short)p.events) & entries[j].mask;
            p.revents = (short)r;
            if (r != 0)
                ready++;
        }
        if (ready > 0) {
            result = ready;
            break;
        }

        uint64_t remaining = UINT64_MAX;
        if (deadline != UINT64_MAX) {
            uint64_t now = HAL_GetNSTicks() / 1'000'000;
            if (now >= deadline) {
                result = 0;
                break;
            }
            remaining = deadline - now;
        }

        int rc = Event::BlockOnWaitNodes(nodePtrs, n, remaining);
        if (rc == -EINTR) {
            result = -EINTR;
            break;
        }
    }

    Event::UnregisterWaitNodes(nodePtrs, n);
    for (size_t j = 0; j < n; j++)
        entries[j].desc->Unref();

    if (nfds > 0 && result >= 0 && !UserWrite(fds, kFDs, nfds * sizeof(PollFD), proc))
        result = -EFAULT;

    delete[] kFDs;
    delete[] entries;
    delete[] nodePtrs;
    return result;
}

int sys_poll(PollFD* fds, nfds_t nfds, int timeout) {
    Thread* current = Thread::GetCurrentThread();
    
    timespec ts;
    const timespec* p = nullptr;
    if (timeout >= 0) {
        ts = { timeout / 1000, (timeout % 1000) * 1000000};
        p = &ts;
    }
    return doPoll(fds, nfds, p, current);
}

int sys_ppoll(PollFD* fds, nfds_t nfds, const timespec* ts, const sigset_t* mask) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();

    timespec kts;
    sigset_t sigset;
    sigset_t savedSet;

    if (ts != nullptr) {
        if (!UserRead(ts, &kts, sizeof(timespec), proc))
            return -EFAULT;
    }

    if (mask != nullptr) {
        if (!UserRead(mask, &sigset, sizeof(sigset), proc))
            return -EFAULT;

        SIGNAL_CLEAR(&sigset, SIGKILL);
        SIGNAL_CLEAR(&sigset, SIGSTOP);
        current->ChangeSignalMask(SIG_SETMASK, &sigset, &savedSet);
    }

    int rc = doPoll(fds, nfds, ts != nullptr ? &kts : nullptr, current);
    if (mask != nullptr) {
        if (rc == -EINTR)
            current->SetReturnSignalMask(&savedSet);
        else
            current->ChangeSignalMask(SIG_SETMASK, &savedSet, nullptr);
    }

    return rc;
}

int sys_fchmodat(int fd, const char* path, size_t pathLen, mode_t mode, int flags) {
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();

    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, fd, path, pathLen, flags, &vnode);
    if (rc < 0)
        return rc;

    if ((flags & AT_SYMLINK_NOFOLLOW) > 0 && vnode->GetType() == FS::VType::LNK)
        rc = -ENOTSUP;
    else
        rc = FS::VFS_Chmod(vnode, mode, proc->GetCred());

    FS::UnrefVNode(vnode);
    return rc;
}

int sys_fchownat(int fd, const char* path, size_t pathLen, UGIDs ids, int flags) {
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();

    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, fd, path, pathLen, flags, &vnode);
    if (rc < 0)
        return rc;

    rc = FS::VFS_Chown(vnode, ids.owner, ids.group, proc->GetCred());

    FS::UnrefVNode(vnode);
    return rc;
}

int sys_unlinkat(int dirFD, const char* path, size_t pathLen, int flags) {
    if ((flags & ~AT_REMOVEDIR) != 0)
        return -EINVAL;
    if (path == nullptr)
        return -EFAULT;
    if (pathLen == 0)
        return -ENOENT;
    if (pathLen > PATH_MAX)
        return -ENAMETOOLONG;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    bool removeDir = (flags & AT_REMOVEDIR) > 0;

    char* kPath = new char[pathLen + 1];
    if (kPath == nullptr)
        return -ENOMEM;
    if (!UserRead(path, kPath, pathLen, proc)) {
        delete[] kPath;
        return -EFAULT;
    }
    kPath[pathLen] = 0;

    size_t len = pathLen;
    bool trailing = false;
    while (len > 1 && kPath[len - 1] == '/') {
        kPath[--len] = 0;
        trailing = true;
    }
    if (len == 1 && kPath[0] == '/') {
        delete[] kPath;
        return removeDir ? -EBUSY : -EISDIR;
    }

    FS::VNode* dirVNode = nullptr;
    if (dirFD == AT_FDCWD)
        dirVNode = proc->GetCWDRef();
    else if (kPath[0] != '/') {
        FileDescriptor* desc = manager->Get(dirFD);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
            bool notDir = desc != nullptr && desc->isOpen();
            if (desc != nullptr)
                desc->Unref();
            delete[] kPath;
            return notDir ? -ENOTDIR : -EBADF;
        }
        dirVNode = desc->GetVNode();
        FS::RefVNode(dirVNode);
        desc->Unref();
    }

    const char* parent;
    const char* name;
    SplitPath(kPath, &parent, &name);
    int rc = FS::VFS_Unlink(parent, name, dirVNode, proc->GetCred(), removeDir, trailing);

    FS::UnrefVNode(dirVNode);
    delete[] kPath;
    return rc;
}

int sys_fcntl(int fd, int op, uint64_t arg) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    // ops that don't require the descriptor
    switch (op) {
    case F_GETFD:
        return manager->GetFDFlags(fd);
    case F_SETFD:
        return manager->SetFDFlags(fd, (int)arg);
    default:
        break;
    }

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    int rc = -EINVAL;

    switch (op) {
    case F_DUPFD_CLOEXEC:
    case F_DUPFD: {
        desc->Ref(); // new slot's reference
        fd_t newFD = manager->Allocate(desc, op == F_DUPFD_CLOEXEC ? FD_CLOEXEC : 0, (fd_t)arg);
        if (newFD < 0)
            desc->Unref();
        rc = newFD;
        break;
    }
    case F_GETFL:
        rc = desc->GetFlags();
        break;
    case F_SETFL:
        rc = desc->SetFlags((int)arg);
        break;
    }

    desc->Unref();
    return rc;
}

int sys_dup(int fd, int flags) {
    if ((flags & ~O_CLOEXEC) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    fd_t newFD = manager->Allocate(desc, (flags & O_CLOEXEC) > 0 ? FD_CLOEXEC : 0);
    if (newFD < 0)
        desc->Unref();
    return newFD;
}

int sys_dup2(int fd, int newFD, int flags) {
    if ((flags & ~O_CLOEXEC) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    if (newFD < 0 || newFD >= MAX_FD)
        return -EBADF;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    if (fd == newFD) {
        desc->Unref();
        return flags != 0 ? -EINVAL : newFD;
    }

    fd_t rc = manager->Replace(newFD, desc, (flags & O_CLOEXEC) > 0 ? FD_CLOEXEC : 0);
    if (rc < 0)
        desc->Unref();
    return rc;
}

int sys_truncate(const char* path, size_t pathLen, off_t size) {
    if (path == nullptr || pathLen == 0 || size < 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    
    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, AT_FDCWD, path, pathLen, 0, &vnode);
    if (rc < 0)
        return rc;

    rc = vnode->Access(ACCESS_W, proc->GetCred());
    if (rc < 0) {
        FS::UnrefVNode(vnode);
        return rc;
    }

    rc = FS::VFS_Truncate(vnode, size, proc->GetCred());
    FS::UnrefVNode(vnode);
    return rc;
}

int sys_ftruncate(int fd, off_t size) {
    if (fd < 0)
        return -EBADF;
    if (size < 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    FileDescriptor* desc = manager->Get(fd);
    if (desc == nullptr || !desc->isOpen()) {
        if (desc != nullptr)
            desc->Unref();
        return -EBADF;
    }

    int flags = desc->GetFlags() & O_ACCMODE;
    if (flags != O_WRONLY && flags != O_RDWR) {
        desc->Unref();
        return -EINVAL;
    }

    FS::VNode* vnode = desc->GetVNode();
    FS::RefVNode(vnode);
    desc->Unref();

    int rc = FS::VFS_Truncate(vnode, size, proc->GetCred());
    FS::UnrefVNode(vnode);
    return rc;
}

int sys_pipe(int* fds, int flags) {
    if ((flags & ~(O_CLOEXEC | O_NONBLOCK)) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* manager = proc->GetFDManager();
    if (manager == nullptr)
        return -ENOSYS;

    Pipe* pipe = new Pipe;
    if (pipe == nullptr)
        return -ENOMEM;
    if (!pipe->Init()) {
        delete pipe;
        return -ENOMEM;
    }

    FileDescriptor* rd = new FileDescriptor(proc, pipe, false);
    FileDescriptor* wr = rd != nullptr ? new FileDescriptor(proc, pipe, true) : nullptr;
    if (rd == nullptr || wr == nullptr) {
        if (rd != nullptr)
            delete rd;
        else
            pipe->Unref();
        if (wr != nullptr)
            delete wr;
        else
            pipe->Unref();
        return -ENOMEM;
    }

    int fdFlags = (flags & O_CLOEXEC) > 0 ? FD_CLOEXEC : 0;
    if (rd->Open(O_RDONLY | (flags & O_NONBLOCK)) < 0 || wr->Open(O_WRONLY | (flags & O_NONBLOCK)) < 0) {
        delete rd;
        delete wr;
        return -EBADF;
    }

    fd_t r = manager->Allocate(rd, fdFlags);
    if (r < 0) {
        delete rd;
        delete wr;
        return (int)r;
    }

    fd_t w = manager->Allocate(wr, fdFlags);
    if (w < 0) {
        manager->Free(r);
        delete wr;
        return (int)w;
    }

    int kFDs[2] = { (int)r, (int)w };
    if (!UserWrite(fds, kFDs, sizeof(kFDs), proc)) {
        manager->Free(r);
        manager->Free(w);
        return -EFAULT;
    }
    return ESUCCESS;
}

int sys_faccessat(int dirfd, const char* path, size_t pathLen, int mode, int flags) {
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_EACCESS)) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();

    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, dirfd, path, pathLen, flags, &vnode);
    if (rc < 0)
        return rc;

    Credential cred = proc->GetCred();
    if ((flags & AT_EACCESS) == 0) { // default behaviour of faccessat is to use uid/gid, but vnode access uses euid/egid
        cred.euid = cred.uid;
        cred.egid = cred.gid;
    }

    rc = vnode->Access(mode, cred);

    FS::UnrefVNode(vnode);
    return rc;
}

mode_t sys_umask(mode_t mode) {
    return Thread::GetCurrentThread()->GetParent()->SwapUMask(mode & 0777);
}

static void StripTrailingSlashs(char* path, bool* trailing) {
    size_t len = strlen(path);
    *trailing = false;
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = 0;
        *trailing = true;
    }
}

int sys_renameat(uint64_t fds, const char* oldPath, size_t oldPathLen, const char* newPath, size_t newPathLen) {
    if (oldPath == nullptr || oldPathLen == 0 || newPath == nullptr || newPathLen == 0)
        return -ENOENT;

    if (oldPathLen > PATH_MAX || newPathLen > PATH_MAX)
        return -ENAMETOOLONG;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* mgr = proc->GetFDManager();
    if (mgr == nullptr)
        return -ENOSYS;

    char* kOldPath = new char[oldPathLen + 1];
    char* kNewPath = new char[newPathLen + 1];

    if (kOldPath == nullptr || kNewPath == nullptr) {
        if (kOldPath != nullptr)
            delete[] kOldPath;
        if (kNewPath != nullptr)
            delete[] kNewPath;
        return -ENOMEM;
    }

    if (!UserRead(oldPath, kOldPath, oldPathLen, proc) || !UserRead(newPath, kNewPath, newPathLen, proc)) {
        delete[] kOldPath;
        delete[] kNewPath;
        return -EFAULT;
    }

    kOldPath[oldPathLen] = 0;
    kNewPath[newPathLen] = 0;

    FS::VNode* vnodes[2] = {nullptr, nullptr}; // 0 = old, 1 = new

    int kFDs[2] = {(int)((fds >> 32) & UINT32_MAX), (int)(fds & UINT32_MAX)};
    char* paths[2] = {kOldPath, kNewPath};
    for (int i = 0; i < 2; i++) {
        if (paths[i][0] == '/')
            continue; // absolute path, skip the dirfd
        if (kFDs[i] == AT_FDCWD) {
            vnodes[i] = proc->GetCWDRef();
            continue;
        }
        FileDescriptor* desc = mgr->Get(kFDs[i]);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::Directory) {
            if (i == 1 && vnodes[0] != nullptr)
                FS::UnrefVNode(vnodes[0]);
            if (desc != nullptr)
                desc->Unref();
            delete[] kOldPath;
            delete[] kNewPath;
            return -EBADF;
        }

        vnodes[i] = desc->GetVNode();
        FS::RefVNode(vnodes[i]);
        desc->Unref();
    }

    bool oldTrailing, newTrailing;
    StripTrailingSlashs(kOldPath, &oldTrailing);
    StripTrailingSlashs(kNewPath, &newTrailing);

    const char* oldParent;
    const char* oldName;
    const char* newParent;
    const char* newName;
    SplitPath(kOldPath, &oldParent, &oldName);
    SplitPath(kNewPath, &newParent, &newName);

    FS::RenameEnd from = {oldParent, oldName, vnodes[0], oldTrailing};
    FS::RenameEnd to   = {newParent, newName, vnodes[1], newTrailing};
    int rc = VFS_Rename(from, to, proc->GetCred());

    for (int i = 0; i < 2; i++)
        FS::UnrefVNode(vnodes[i]);

    delete[] kOldPath;
    delete[] kNewPath;

    return rc;
}

ssize_t sys_readlinkat(int dirfd, const char* path, size_t pathLen, void* buffer, size_t maxSize) {
    if (path == nullptr || buffer == nullptr)
        return -EFAULT;
    if (maxSize == 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();

    FS::VNode* vnode = nullptr;
    int rc = ResolveVNodeAt(proc, dirfd, path, pathLen, AT_SYMLINK_NOFOLLOW, &vnode);
    if (rc < 0)
        return rc;

    if (vnode->GetType() != FS::VType::LNK) {
        FS::UnrefVNode(vnode);
        return -EINVAL;
    }

    char target[PATH_MAX + 1];
    rc = vnode->ReadLink(target, PATH_MAX + 1, proc->GetCred());
    FS::UnrefVNode(vnode);
    if (rc < 0)
        return rc;

    size_t len = strlen(target);
    size_t n = len < maxSize ? len : maxSize;
    if (!UserWrite(buffer, target, n, proc))
        return -EFAULT;

    return (ssize_t)n;
}
