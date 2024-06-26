/*
Copyright (©) 2022-2024  Frosty515

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

#include "Thread.hpp"
#include "Scheduler.hpp"
#include "Semaphore.hpp"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <util.h>

#include <file.h>

#include <fs/TempFS/TempFSInode.hpp>

#include <fs/FileStream.hpp>
#include <fs/DirectoryStream.hpp>

namespace Scheduling {

    Thread::Thread(Process* parent, ThreadEntry_t entry, void* entry_data, uint8_t flags, tid_t TID) : m_Parent(parent), m_entry(entry), m_entry_data(entry_data), m_flags(flags), m_stack(0), m_cleanup({nullptr, nullptr}), m_FDManager(), m_TID(TID), m_sleeping(false), m_remaining_sleep_time(0), m_idle(false), m_blocked(false), m_working_directory(nullptr) {
        memset(&m_regs, 0, DIV_ROUNDUP(sizeof(m_regs), 8));
        m_frame.kernel_stack = (uint64_t)g_KPM->AllocatePages(KERNEL_STACK_SIZE >> 12, PagePermissions::READ_WRITE) + KERNEL_STACK_SIZE; // FIXME: use actual page size
    }

    Thread::~Thread() {
        g_KPM->FreePages((void*)(m_frame.kernel_stack - KERNEL_STACK_SIZE));
        if (m_working_directory != nullptr)
            delete m_working_directory;
    }

    void Thread::SetEntry(ThreadEntry_t entry, void* entry_data) {
        m_entry = entry;
        m_entry_data = entry_data;
    }

    void Thread::SetFlags(uint8_t flags) {
        m_flags = flags;
    }

    void Thread::SetParent(Process* parent) {
        m_Parent = parent;
    }

    void Thread::SetStack(uint64_t stack) {
        m_stack = stack;
        if (m_Parent != nullptr) {
            if (m_Parent->GetPriority() == Priority::KERNEL)
                m_frame.kernel_stack = stack;
        }
    }

    void Thread::SetCleanupFunction(ThreadCleanup_t cleanup) {
        m_cleanup = cleanup;
    }

    ThreadEntry_t Thread::GetEntry() const {
        return m_entry;
    }

    void* Thread::GetEntryData() const {
        return m_entry_data;
    }

    uint8_t Thread::GetFlags() const {
        return m_flags;
    }

    Process* Thread::GetParent() const {
        return m_Parent;
    }

    CPU_Registers* Thread::GetCPURegisters() const {
        return &m_regs;
    }

    uint64_t Thread::GetStack() const {
        return m_stack;
    }

    uint64_t Thread::GetKernelStack() const {
        return m_frame.kernel_stack;
    }

    ThreadCleanup_t Thread::GetCleanupFunction() const {
        return m_cleanup;
    } 

    Thread::Register_Frame* Thread::GetStackRegisterFrame() const {
        return &m_frame;
    }

    void Thread::Start() {
        m_FDManager.ReserveFileDescriptor(FileDescriptorType::TTY, g_CurrentTTY, FileDescriptorMode::READ, 0); // not properly supported yet, but here to reserve the file descriptor ID
        Position pos = g_CurrentTTY->GetVGADevice()->GetCursorPosition(); // save the current position
        m_FDManager.ReserveFileDescriptor(FileDescriptorType::TTY, g_CurrentTTY, FileDescriptorMode::WRITE, 1);
        m_FDManager.ReserveFileDescriptor(FileDescriptorType::TTY, g_CurrentTTY, FileDescriptorMode::WRITE, 2);
        g_CurrentTTY->GetVGADevice()->SetCursorPosition(pos); // restore the position
        m_FDManager.ReserveFileDescriptor(FileDescriptorType::DEBUG, nullptr, FileDescriptorMode::APPEND, 3);
        Scheduler::ScheduleThread(this);
    }

    fd_t Thread::sys_open(const char* path, unsigned long flags, unsigned short mode) {
        if (m_Parent == nullptr || !m_Parent->ValidateStringRead(path))
            return -EFAULT;

        bool create = flags & O_CREATE;
        if (create)
            flags &= ~O_CREATE;

        FileDescriptorMode new_mode;
        uint8_t vfs_modes;
        if (flags == O_READ) {
            new_mode = FileDescriptorMode::READ;
            vfs_modes = VFS_READ;
        }
        else if (flags == O_APPEND) {
            new_mode = FileDescriptorMode::APPEND;
            vfs_modes = VFS_WRITE;
        }
        else if (flags == O_WRITE) {
            new_mode = FileDescriptorMode::WRITE;
            vfs_modes = VFS_WRITE;
        }
        else if (flags == (O_READ | O_WRITE)) {
            new_mode = FileDescriptorMode::READ_WRITE;
            vfs_modes = VFS_READ | VFS_WRITE;
        }
        else
            return -EINVAL;

        FilePrivilegeLevel current_privilege = {m_Parent->GetEUID(), m_Parent->GetEGID(), 0};

        bool valid_path = g_VFS->IsValidPath(path, m_working_directory);
        if (create) {
            if (!valid_path) {
                char* end = strrchr(path, PATH_SEPARATOR);
                char* child_start = (char*)((uint64_t)end + sizeof(char));
                char const* parent;
                if (end != nullptr) {
                    size_t parent_name_size = (size_t)child_start - (size_t)path;
                    char* i_parent = new char[parent_name_size + 1];
                    memcpy(i_parent, path, parent_name_size);
                    i_parent[parent_name_size] = '\0';
                    if (!g_VFS->IsValidPath(i_parent, m_working_directory)) {
                        delete[] i_parent;
                        return -ENOENT;
                    }
                    parent = i_parent;
                }
                else
                    parent = "/";
                int rc = g_VFS->CreateFile(current_privilege, parent, end == nullptr ? path : child_start, 0, false, {m_Parent->GetEUID(), m_Parent->GetEGID(), mode});
                if (end != nullptr)
                    delete[] parent;
                if (rc != ESUCCESS)
                    return rc;
            }
        }
        else if (!valid_path)
            return -ENOENT;

        fd_t fd;
        FileStream* fstream = nullptr;
        DirectoryStream* dstream = nullptr;

        int status = 0;
        Inode* inode = g_VFS->GetInode(path, m_working_directory, nullptr, nullptr, &status);
        if (inode == nullptr) {
            if (status == ESUCCESS) { // there is no inode for the root of a mountpoint, so things get complicated
                int status = 0;
                dstream = g_VFS->OpenDirectoryStream(current_privilege, path, vfs_modes, m_working_directory, &status);
                if (dstream == nullptr)
                    return status;

                fd = m_FDManager.AllocateFileDescriptor(FileDescriptorType::DIRECTORY_STREAM, dstream, new_mode);
                if (fd < 0) {
                    (void)(g_VFS->CloseDirectoryStream(dstream)); // we are cleaning up, return value is irrelevant
                    return -ENOMEM;
                }
            }
            else
                return -ENOENT; // this case *should* already have been handled, but is checked here for safety.
        }
        else {
            switch (inode->GetType()) {
            case InodeType::File: {
                fstream = g_VFS->OpenStream(current_privilege, path, vfs_modes, m_working_directory, &status);
                if (fstream == nullptr)
                    return status;

                fd = m_FDManager.AllocateFileDescriptor(FileDescriptorType::FILE_STREAM, fstream, new_mode);
                if (fd < 0) {
                    (void)(g_VFS->CloseStream(fstream)); // we are cleaning up, return value is irrelevant
                    return -ENOMEM;
                }
                break;
            }
            case InodeType::Folder: {
                dstream = g_VFS->OpenDirectoryStream(current_privilege, path, vfs_modes, m_working_directory, &status);
                if (dstream == nullptr)
                    return status;

                fd = m_FDManager.AllocateFileDescriptor(FileDescriptorType::DIRECTORY_STREAM, dstream, new_mode);
                if (fd < 0) {
                    (void)(g_VFS->CloseDirectoryStream(dstream)); // we are cleaning up, return value is irrelevant
                    return -ENOMEM;
                }
                break;
            }
            default:
                return -ENOSYS;
            }
        }

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(fd);
        if (descriptor == nullptr) {
            (void)(m_FDManager.FreeFileDescriptor(fd));
            if (fstream != nullptr)
                (void)(g_VFS->CloseStream(fstream)); // we are cleaning up, return value is irrelevant
            else if (dstream != nullptr)
                (void)(g_VFS->CloseDirectoryStream(dstream)); // we are cleaning up, return value is irrelevant
            assert(false); // something has gone seriously wrong. just crash. FIXME: handle this case better
        }

        if (!descriptor->Open()) {
            assert(false); // something has gone seriously wrong. just crash. FIXME: handle this case better
        }

        return fd;
    }

    long Thread::sys_read(fd_t file, void* buf, unsigned long count) {
        if (m_Parent == nullptr || !m_Parent->ValidateWrite(buf, count))
            return -EFAULT;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr)
            return -EBADF;
        
        int status = 0;
        long rc = descriptor->Read((uint8_t*)buf, count, &status);
        if (rc == 0) {
            if (status == -EINVAL)
                return EOF;
            else
                return status;
        }
        return rc;
    }

    long Thread::sys_write(fd_t file, const void* buf, unsigned long count) {
        if (count == 0)
            return ESUCCESS; // no point as there is nothing to write

        if (m_Parent == nullptr || !m_Parent->ValidateRead(buf, count))
            return -EFAULT;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr)
            return -EBADF;
        
        int status = 0;
        long rc = descriptor->Write((uint8_t*)buf, count, &status);
        if (rc == 0) {
            if (status == -EINVAL)
                return EOF;
            else
                return status;
        }

        if (descriptor->GetType() == FileDescriptorType::TTY)
            ((TTY*)descriptor->GetData())->GetVGADevice()->SwapBuffers(false);

        return rc;
    }

    int Thread::sys_close(fd_t file) {
        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr)
            return -EBADF;

        (void)descriptor->Close(); // return value is irrelevant
        if (descriptor->GetType() == FileDescriptorType::FILE_STREAM) {
            FileStream* stream = (FileStream*)descriptor->GetData();
            if (stream != nullptr)
                delete stream;
        }
        (void)m_FDManager.FreeFileDescriptor(file); // return value is irrelevant
        delete descriptor;
        return ESUCCESS;
    }

    long Thread::sys_seek(fd_t file, long offset, long whence) {
        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr || descriptor->GetType() != FileDescriptorType::FILE_STREAM)
            return -EBADF;

        FileStream* stream = (FileStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        long i_offset;
        if (whence == SEEK_SET) {
            if (offset < 0)
                return -EINVAL;
            i_offset = offset;
        }
        else if (whence == SEEK_CUR)
                i_offset = stream->GetOffset() + offset;
        else if (whence == SEEK_END) {
            Inode* inode = stream->GetInode();
            FileSystem* fs = stream->GetFileSystem();
            if (inode == nullptr || fs == nullptr)
                return -EBADF;
            switch (fs->GetType()) {
            case FileSystemType::TMPFS: {
                TempFS::TempFSInode* tempfs_inode = (TempFS::TempFSInode*)inode;
                if (tempfs_inode->GetType() == InodeType::File)
                    i_offset = tempfs_inode->GetSize() - 1 + offset;
                else
                    return -EINVAL;
                break;
            }
            default:
                return -ENOSYS;
            }
        }
        else
            return -EINVAL;

        int rc = descriptor->Seek((uint64_t)i_offset);
        if (rc != ESUCCESS)
            return rc;

        return i_offset;
    }

    int Thread::sys_stat(const char* path, struct stat_buf* buf) {
        if (m_Parent == nullptr || !m_Parent->ValidateWrite(buf, sizeof(struct stat_buf)) || !m_Parent->ValidateStringRead(path))
            return -EFAULT;

        // Validate path
        if (!g_VFS->IsValidPath(path, m_working_directory))
            return -ENOENT;

        FileSystem* fs = nullptr;
        int status = 0;
        Inode* inode = g_VFS->GetInode(path, m_working_directory, &fs, nullptr, &status);
        if ((inode == nullptr && status != ESUCCESS) || fs == nullptr)
            return -ENOENT;

        if (inode == nullptr) {
            struct stat_buf buffer;

            buffer.st_uid = 0;
            buffer.st_gid = 0;
            buffer.st_mode = 0;
            buffer.st_type = DT_DIR;
            buffer.st_size = 0;

            memcpy(buf, &buffer, sizeof(struct stat_buf));
            return ESUCCESS;
        }

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
        
        struct stat_buf buffer;

        buffer.st_uid = privilege.UID;
        buffer.st_gid = privilege.GID;
        buffer.st_mode = privilege.ACL;

        switch (inode->GetType()) {
        case InodeType::File:
            buffer.st_type = DT_FILE;
            break;
        case InodeType::Folder:
            buffer.st_type = DT_DIR;
            break;
        case InodeType::SymLink:
            buffer.st_type = DT_SYMLNK;
            break;
        }

        switch (fs->GetType()) {
        case FileSystemType::TMPFS: {
            TempFS::TempFSInode* tempfs_inode = (TempFS::TempFSInode*)inode;
            if (tempfs_inode->GetType() == InodeType::File)
                buffer.st_size = tempfs_inode->GetSize();
            else
                buffer.st_size = 0;
            break;
        }
        default:
            return -ENOSYS;
        }

        memcpy(buf, &buffer, sizeof(struct stat_buf));
        return ESUCCESS;
    }

    int Thread::sys_fstat(fd_t file, struct stat_buf* buf) {
        if (m_Parent == nullptr || !m_Parent->ValidateWrite(buf, sizeof(struct stat_buf)))
            return -EFAULT;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr || descriptor->GetType() != FileDescriptorType::FILE_STREAM)
            return -EBADF;

        FileStream* stream = (FileStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        Inode* inode = stream->GetInode();
        FileSystem* fs = stream->GetFileSystem();
        if (inode == nullptr || fs == nullptr)
            return -EBADF;

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
        
        struct stat_buf buffer;

        buffer.st_uid = privilege.UID;
        buffer.st_gid = privilege.GID;
        buffer.st_mode = privilege.ACL;

        switch (inode->GetType()) {
        case InodeType::File:
            buffer.st_type = DT_FILE;
            break;
        case InodeType::Folder:
            buffer.st_type = DT_DIR;
            break;
        case InodeType::SymLink:
            buffer.st_type = DT_SYMLNK;
            break;
        }

        switch (fs->GetType()) {
        case FileSystemType::TMPFS: {
            TempFS::TempFSInode* tempfs_inode = (TempFS::TempFSInode*)inode;
            if (tempfs_inode->GetType() == InodeType::File)
                buffer.st_size = tempfs_inode->GetSize();
            else
                buffer.st_size = 0;
            break;
        }
        default:
            return -ENOSYS;
        }

        memcpy(buf, &buffer, sizeof(struct stat_buf));
        return ESUCCESS;
    }

    int Thread::sys_chown(const char* path, unsigned int uid, unsigned int gid) {
        if (m_Parent == nullptr || !m_Parent->ValidateStringRead(path))
            return -EFAULT;
        
        if (uid == (unsigned int)-1 && gid == (unsigned int)-1)
            return ESUCCESS; // no point checking anything if we are not changing anything

        // Ensure we are UID 0
        if (m_Parent->GetEUID() != 0)
            return -EPERM;

        // Validate path
        if (!g_VFS->IsValidPath(path, m_working_directory))
            return -ENOENT;

        Inode* inode = g_VFS->GetInode(path, m_working_directory);
        if (inode == nullptr)
            return -ENOENT;

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
        if (uid != (unsigned int)-1)
            privilege.UID = uid;
        if (gid != (unsigned int)-1)
            privilege.GID = gid;
        inode->SetPrivilegeLevel(privilege);
        return ESUCCESS;
    }

    int Thread::sys_fchown(fd_t file, unsigned int uid, unsigned int gid) {
        if (m_Parent == nullptr)
            return -EFAULT;

        if (uid == (unsigned int)-1 && gid == (unsigned int)-1)
            return ESUCCESS; // no point checking anything if we are not changing anything

        // Ensure we are UID 0
        if (m_Parent->GetEUID() != 0)
            return -EPERM;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr || descriptor->GetType() != FileDescriptorType::FILE_STREAM)
            return -EBADF;

        FileStream* stream = (FileStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        Inode* inode = stream->GetInode();
        if (inode == nullptr)
            return -EBADF;

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
        if (uid != (unsigned int)-1)
            privilege.UID = uid;
        if (gid != (unsigned int)-1)
            privilege.GID = gid;
        inode->SetPrivilegeLevel(privilege);
        return ESUCCESS;
    }

    int Thread::sys_chmod(const char* path, unsigned short mode) {
        if (m_Parent == nullptr || !m_Parent->ValidateStringRead(path))
            return -EFAULT;

        // Validate path
        if (!g_VFS->IsValidPath(path, m_working_directory))
            return -ENOENT;

        Inode* inode = g_VFS->GetInode(path, m_working_directory);
        if (inode == nullptr)
            return -ENOENT;

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
        
        // Ensure we are the owner or group owner of the file or we are UID/GID 0
        uint32_t uid = m_Parent->GetEUID();
        uint32_t gid = m_Parent->GetEGID();
        if ((privilege.UID != uid && uid != 0) || (privilege.GID != gid && gid != 0))
            return -EPERM;

        privilege.ACL = mode;
        inode->SetPrivilegeLevel(privilege);
        return ESUCCESS;
    }

    int Thread::sys_fchmod(fd_t file, unsigned short mode) {
        if (m_Parent == nullptr)
            return -EFAULT;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr || descriptor->GetType() != FileDescriptorType::FILE_STREAM)
            return -EBADF;

        FileStream* stream = (FileStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        Inode* inode = stream->GetInode();
        if (inode == nullptr)
            return -EBADF;

        FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();

        // Ensure we are the owner or group owner of the file or we are UID/GID 0
        uint32_t uid = m_Parent->GetEUID();
        uint32_t gid = m_Parent->GetEGID();
        if ((privilege.UID != uid && uid != 0) || (privilege.GID != gid && gid != 0))
            return -EPERM;
        
        privilege.ACL = mode;
        inode->SetPrivilegeLevel(privilege);
        return ESUCCESS;
    }

    void Thread::sys_sleep(unsigned long s) {
        Scheduler::SleepThread(this, s * 1000);
    }

    void Thread::sys_msleep(unsigned long ms) {
        Scheduler::SleepThread(this, ms);
    }

    int Thread::sys_getdirents(fd_t file, struct dirent* buf, unsigned long count) {
        if (m_Parent == nullptr || !m_Parent->ValidateWrite(buf, count * sizeof(struct dirent)))
            return -EFAULT;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr)
            return -EBADF;

        if (descriptor->GetType() != FileDescriptorType::DIRECTORY_STREAM)
            return -ENOTDIR;

        DirectoryStream* stream = (DirectoryStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        {
            Inode* parent_inode = stream->GetInode();
            if (parent_inode == nullptr) {
                FileSystem* fs = stream->GetFileSystem();
                if (fs == nullptr)
                    return -EBADF;
                if ((count + stream->GetOffset()) > fs->GetRootInodeCount())
                    return -EINVAL;
            }
            else {
                if ((count + stream->GetOffset()) > parent_inode->GetChildCount())
                    return -EINVAL;
            }
        }

        size_t inode_size = 0;
        switch (stream->GetFileSystemType()) {
        case FileSystemType::TMPFS:
            inode_size = sizeof(TempFS::TempFSInode);
            break;
        default:
            return -ENOSYS;
        }

        for (uint64_t i = 0; i < count; i++) {
            Inode* inode = (Inode*)kcalloc(1, inode_size);
            if (inode == nullptr)
                return -ENOMEM;
            int rc = descriptor->Read((uint8_t*)inode, inode_size);
            if (rc <= 0) {
                kfree(inode);
                if (rc == 0)
                    return i;
                return rc;
            }
            struct dirent i_dirent;
            memset(&i_dirent, 0, sizeof(struct dirent));
            switch (inode->GetRealType()) {
            case InodeType::File:
                i_dirent.d_type = DT_FILE;
                break;
            case InodeType::Folder:
                i_dirent.d_type = DT_DIR;
                break;
            case InodeType::SymLink:
                i_dirent.d_type = DT_SYMLNK;
                break;
            }

            FilePrivilegeLevel privilege = inode->GetPrivilegeLevel();
            
            i_dirent.d_uid = privilege.UID;
            i_dirent.d_gid = privilege.GID;
            i_dirent.d_mode = privilege.ACL;

            switch (stream->GetFileSystemType()) {
            case FileSystemType::TMPFS: {
                TempFS::TempFSInode* tmp_inode = (TempFS::TempFSInode*)inode;
                i_dirent.d_size = tmp_inode->GetSize(); // will return 0 if type is not file
                break;
            }
            default:
                return -ENOSYS;
            }

            strncpy(i_dirent.d_name, inode->GetName(), 255);

            memcpy(&(buf[i]), &i_dirent, sizeof(struct dirent));

            kfree(inode);
        }

        return ESUCCESS;
    }

    int Thread::sys_chdir(const char* path) {
        if (m_Parent == nullptr || !m_Parent->ValidateStringRead(path))
            return -EFAULT;

        if (m_working_directory == nullptr)
            return -ENOSYS;

        // Validate path
        if (!g_VFS->IsValidPath(path, m_working_directory))
            return -ENOENT;

        VFS_MountPoint* mountpoint = nullptr;
        int status = 0;
        Inode* inode = g_VFS->GetInode(path, m_working_directory, nullptr, &mountpoint, &status);
        if (mountpoint == nullptr || (inode == nullptr && status != ESUCCESS))
            return -ENOENT;

        m_working_directory->inode = inode;
        m_working_directory->mountpoint = mountpoint;

        if (m_Parent->IsMainThread(this)) {
            VFS_WorkingDirectory* parent_wd = m_Parent->GetDefaultWorkingDirectory();
            parent_wd->inode = inode;
            parent_wd->mountpoint = mountpoint;
        }

        return ESUCCESS;
    }

    int Thread::sys_fchdir(fd_t file) {
        if (m_working_directory == nullptr)
            return -ENOSYS;

        FileDescriptor* descriptor = m_FDManager.GetFileDescriptor(file);
        if (descriptor == nullptr || descriptor->GetType() != FileDescriptorType::DIRECTORY_STREAM)
            return -EBADF;

        DirectoryStream* stream = (DirectoryStream*)descriptor->GetData();
        if (stream == nullptr)
            return -EBADF;

        Inode* inode = stream->GetInode();
        FileSystem* fs = stream->GetFileSystem();
        if (inode == nullptr && fs == nullptr)
            return -EBADF;
        VFS_MountPoint* mountpoint = g_VFS->GetMountPoint(fs);
        if (mountpoint == nullptr)
            return -EBADF; // should not be null, but this check is here just in case.

        m_working_directory->inode = inode;
        m_working_directory->mountpoint = mountpoint;

        if (m_Parent->IsMainThread(this)) {
            VFS_WorkingDirectory* parent_wd = m_Parent->GetDefaultWorkingDirectory();
            parent_wd->inode = inode;
            parent_wd->mountpoint = mountpoint;
        }

        return ESUCCESS;
    }

    void Thread::PrintInfo(fd_t file) const {
        fprintf(file, "Thread %lp\n", this);
        fprintf(file, "Entry: %lp\n", m_entry);
        fprintf(file, "Entry data: %lp\n", m_entry_data);
        fprintf(file, "Flags: %u\n", m_flags);
        fprintf(file, "Parent: %lp\n", m_Parent);
        fprintf(file, "Stack: %lp\n", m_stack);
        fprintf(file, "Kernel stack: %lp\n", m_frame.kernel_stack);
        fprintf(file, "Cleanup function: %lp\n", m_cleanup.function);
        fprintf(file, "Cleanup data: %lp\n", m_cleanup.data);
        fprintf(file, "Registers:\n");
        fprintf(file, "RAX: %016lx  ", m_regs.RAX);
        fprintf(file, "RBX: %016lx  ", m_regs.RBX);
        fprintf(file, "RCX: %016lx  ", m_regs.RCX);
        fprintf(file, "RDX: %016lx\n", m_regs.RDX);
        fprintf(file, "RSP: %016lx  ", m_regs.RSP);
        fprintf(file, "RBP: %016lx  ", m_regs.RBP);
        fprintf(file, "RDI: %016lx  ", m_regs.RDI);
        fprintf(file, "RSI: %016lx\n", m_regs.RSI);
        fprintf(file, "R8 : %016lx  ", m_regs.R8);
        fprintf(file, "R9 : %016lx  ", m_regs.R9);
        fprintf(file, "R10: %016lx  ", m_regs.R10);
        fprintf(file, "R11: %016lx\n", m_regs.R11);
        fprintf(file, "R12: %016lx  ", m_regs.R12);
        fprintf(file, "R13: %016lx  ", m_regs.R13);
        fprintf(file, "R14: %016lx  ", m_regs.R14);
        fprintf(file, "R15: %016lx\n", m_regs.R15);
        fprintf(file, "RIP: %016lx  ", m_regs.RIP);
        fprintf(file, "RFLAGS: %016lx\n", m_regs.RFLAGS);
        fprintf(file, "CS: %04lx  ", m_regs.CS);
        fprintf(file, "DS: %04lx  ", m_regs.DS);
        fprintf(file, "SS: %04lx  ", m_regs.DS);
        fprintf(file, "ES: %04lx  ", m_regs.DS);
        fprintf(file, "FS: %04lx  ", m_regs.DS);
        fprintf(file, "GS: %04lx\n", m_regs.DS);
        fprintf(file, "CR3: %016lx\n", m_regs.CR3);
    }

    void Thread::SetTID(tid_t TID) {
        m_TID = TID;
    }

    tid_t Thread::GetTID() const {
        return m_TID;
    }

    bool Thread::IsSleeping() const {
        return m_sleeping;
    }

    void Thread::SetSleeping(bool sleeping) {
        m_sleeping = sleeping;
    }

    uint64_t Thread::GetRemainingSleepTime() const {
        return m_remaining_sleep_time;
    }

    void Thread::SetRemainingSleepTime(uint64_t remaining_sleep_time) {
        m_remaining_sleep_time = remaining_sleep_time;
    }

    bool Thread::IsIdle() const {
        return m_idle;
    }

    void Thread::SetIdle(bool idle) {
        m_idle = idle;
    }

    bool Thread::IsBlocked() const {
        return m_blocked;
    }

    void Thread::SetBlocked(bool blocked) {
        m_blocked = blocked;
    }

    VFS_WorkingDirectory* Thread::GetWorkingDirectory() const {
        return m_working_directory;
    }

    void Thread::SetWorkingDirectory(VFS_WorkingDirectory* working_directory) {
        m_working_directory = working_directory;
    }

    Thread* Thread::GetNextThread() {
        return m_next_thread;
    }

    Thread* Thread::GetPreviousThread() {
        return m_previous_thread;
    }

    void Thread::SetNextThread(Thread* next_thread) {
        m_next_thread = next_thread;
    }

    void Thread::SetPreviousThread(Thread* previous_thread) {
        m_previous_thread = previous_thread;
    }
}