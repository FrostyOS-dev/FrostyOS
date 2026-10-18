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

#ifndef _FILE_DESCRIPTOR_MANAGER_HPP
#define _FILE_DESCRIPTOR_MANAGER_HPP

#include <stddef.h>
#include <stdint.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/AVLTree.hpp>

#include <Scheduling/Mutex.hpp>

#define INITIAL_FD_TABLE_SIZE 128
#define MAX_FD 65536

typedef long fd_t;

class FileDescriptor;

struct FDTableEntry {
    FileDescriptor* desc;
    int flags;
};

class FileDescriptorManager {
public:
    FileDescriptorManager();
    ~FileDescriptorManager();

    bool Init();
    void Delete();

    fd_t Allocate(FileDescriptor* desc, int fdFlags = 0, fd_t minFD = 0);
    bool Free(fd_t fd, FileDescriptor** descOut = nullptr);

    fd_t Replace(fd_t fd, FileDescriptor* desc, int fdFlags = 0);

    bool ReserveFD(fd_t fd, FileDescriptor* desc, int fdFlags = 0);

    FileDescriptor* Get(fd_t fd, int* fdFlags = nullptr);

    int GetFDFlags(fd_t fd);
    int SetFDFlags(fd_t fd, int flags);

    bool Fork(FileDescriptorManager* other);
    bool Exec(FileDescriptorManager* other);

private:
    bool CopyFrom(FileDescriptorManager* other, bool forExec);
    bool GrowLocked(uint64_t bits);
    fd_t AllocateLocked(fd_t minFD);

    AVLTree::wAVLTree<fd_t, FDTableEntry*> m_currentFDs;
    RawBitmap m_bitmap;
};

#endif /* _FILE_DESCRIPTOR_MANAGER_HPP */