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

#include "FDManager.hpp"
#include "FileDescriptor.hpp"

#include <errno.h>
#include <string.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/AVLTree.hpp>

#include <Scheduling/Mutex.hpp>

FileDescriptorManager::FileDescriptorManager() {

}

FileDescriptorManager::~FileDescriptorManager() {

}

bool FileDescriptorManager::Init() {
    uint8_t* buffer = new uint8_t[INITIAL_FD_TABLE_SIZE / 8];
    if (buffer == nullptr)
        return false;
    m_bitmapLock.Lock();
    m_bitmap.SetBuffer(buffer);
    m_bitmap.SetSize(INITIAL_FD_TABLE_SIZE / 8);
    m_bitmapLock.Unlock();
    return true;
}

void FileDescriptorManager::Delete() {
    m_bitmapLock.Lock();
    delete[] m_bitmap.GetBuffer();
    m_bitmap.SetBuffer(nullptr);
    m_bitmap.SetSize(0);
    m_bitmapLock.Unlock();

    m_currentFDs.lock();
    m_currentFDs.forEach([](void*, fd_t, FileDescriptor* desc) -> bool {
        if (desc != nullptr) {
            if (desc->isOpen())
                desc->Close();
            delete desc;
        }
        return true;
    }, nullptr);
    m_currentFDs.Clear();
    m_currentFDs.unlock();
}

fd_t FileDescriptorManager::Allocate(FileDescriptor* desc) {
    m_bitmapLock.Lock();
    for (uint64_t i = 0; i < m_bitmap.GetSize() * 8; i++) {
        if (!m_bitmap.Get(i)) {
            m_bitmap.Set(i, true);
            m_bitmapLock.Unlock();
            m_currentFDs.lock();
            m_currentFDs.Insert(i, desc);
            m_currentFDs.unlock();
            return i;
        }
    }

    // All current FDs are allocated

    if (m_bitmap.GetSize() * 8 == MAX_FD) {
        m_bitmapLock.Unlock();
        return -EMFILE; // no more FDs are allowed
    }

    size_t currentSize = m_bitmap.GetSize();
    uint8_t* buffer = new uint8_t[currentSize + INITIAL_FD_TABLE_SIZE / 8];
    if (buffer == nullptr) {
        m_bitmapLock.Unlock();
        return -ENOMEM;
    }

    memcpy(buffer, m_bitmap.GetBuffer(), currentSize);

    delete[] m_bitmap.GetBuffer();

    m_bitmap.SetBuffer(buffer);
    m_bitmap.SetSize(currentSize + INITIAL_FD_TABLE_SIZE / 8);
    
    m_bitmap.Set(currentSize * 8, true); // currentSize * 8 is the first fd in the newly expanded region
    m_bitmapLock.Unlock();

    m_currentFDs.lock();
    m_currentFDs.Insert(currentSize * 8, desc);
    m_currentFDs.unlock();
    return currentSize * 8;
}

bool FileDescriptorManager::Free(fd_t fd, FileDescriptor** descOut) {
    m_currentFDs.lock();

    AVLTree::wAVLTreeNode* node = m_currentFDs.FindNode(fd);
    if (node == nullptr) {
        m_currentFDs.unlock();
        return false;
    }

    if (descOut != nullptr)
        *descOut = (FileDescriptor*)node->value;

    m_currentFDs.RemoveNode(node);
    m_currentFDs.unlock();

    m_bitmapLock.Lock();
    m_bitmap.Set(fd, false);
    m_bitmapLock.Unlock();

    return true;
}

bool FileDescriptorManager::ReserveFD(fd_t fd, FileDescriptor* desc) {
    m_bitmapLock.Lock();
    if (m_bitmap.Get(fd)) {
        m_bitmapLock.Unlock();
        return false;
    }
    m_bitmap.Set(fd, true);
    m_bitmapLock.Unlock();

    m_currentFDs.lock();
    m_currentFDs.Insert(fd, desc);
    m_currentFDs.unlock();

    return true;
}

FileDescriptor* FileDescriptorManager::Get(fd_t fd) {
    m_currentFDs.lock();
    FileDescriptor* fDesc = m_currentFDs.Find(fd);
    m_currentFDs.unlock();
    return fDesc;
}

bool FileDescriptorManager::Fork(FileDescriptorManager* other, Process* newProc) {
    other->m_bitmapLock.Lock();
    size_t size = other->m_bitmap.GetSize();

    uint8_t* buffer = new uint8_t[size];
    if (buffer == nullptr)
        return false;
    memcpy(buffer, other->m_bitmap.GetBuffer(), size);
    other->m_bitmapLock.Unlock();
    
    m_bitmapLock.Lock();
    m_bitmap.SetBuffer(buffer);
    m_bitmap.SetSize(size);
    m_bitmapLock.Unlock();

    other->m_currentFDs.lock();
    m_currentFDs.lock();
    struct Data {
        FileDescriptorManager* current;
        FileDescriptorManager* other;
        Process* newProc;
        bool success;
    } data = {this, other, newProc, true};
    other->m_currentFDs.forEach([](void* data, fd_t fd, FileDescriptor* desc) -> bool {
        Data* d = static_cast<Data*>(data);
        FileDescriptor* newDesc = new FileDescriptor;
        if (newDesc == nullptr || !newDesc->Fork(desc, d->newProc)) {
            if (newDesc != nullptr)
                delete newDesc;
            d->success = false;
            return false;
        }
        d->current->m_currentFDs.Insert(fd, newDesc);

        return true;
    }, &data);
    other->m_currentFDs.unlock();
    if (!data.success) {
        m_currentFDs.forEach([](void*, fd_t, FileDescriptor* desc) -> bool {
            if (desc != nullptr) {
                if (desc->isOpen())
                    desc->Close();
                delete desc;
            }
            return true;
        }, nullptr);
        m_currentFDs.Clear();
    }
    m_currentFDs.unlock();
    if (!data.success) {
        m_bitmapLock.Lock();
        delete[] m_bitmap.GetBuffer();
        m_bitmap.SetBuffer(nullptr);
        m_bitmap.SetSize(0);
        m_bitmapLock.Unlock();
        return false;
    }

    return true;
}
