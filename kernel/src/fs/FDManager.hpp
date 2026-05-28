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

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/AVLTree.hpp>

#include <Scheduling/Mutex.hpp>

#define INITIAL_FD_TABLE_SIZE 128
#define MAX_FD 65536

typedef long fd_t;

class FileDescriptor;

class FileDescriptorManager {
public:
    FileDescriptorManager();
    ~FileDescriptorManager();

    bool Init();
    void Delete();

    fd_t Allocate(FileDescriptor* desc);
    bool Free(fd_t fd, FileDescriptor** descOut = nullptr); // destruction of the underlying FileDescriptor is the responsibility of the caller

    bool ReserveFD(fd_t fd, FileDescriptor* desc);

    FileDescriptor* Get(fd_t fd);

private:
    AVLTree::wAVLTree<fd_t, FileDescriptor*> m_currentFDs;
    RawBitmap m_bitmap;
    Mutex m_bitmapLock;
};

#endif /* _FILE_DESCRIPTOR_MANAGER_HPP */