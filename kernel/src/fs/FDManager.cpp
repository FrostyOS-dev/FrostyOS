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
#include <util.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/AVLTree.hpp>

#include <Scheduling/Mutex.hpp>

#include <SystemCalls/File.hpp>

FileDescriptorManager::FileDescriptorManager() {

}

FileDescriptorManager::~FileDescriptorManager() {

}

bool FileDescriptorManager::Init() {
    uint8_t* buffer = new uint8_t[INITIAL_FD_TABLE_SIZE / 8]; // new clears the buffer
    if (buffer == nullptr)
        return false;

    m_currentFDs.lock();
    delete[] m_bitmap.GetBuffer();
    m_bitmap.SetBuffer(buffer);
    m_bitmap.SetSize(INITIAL_FD_TABLE_SIZE / 8);
    m_currentFDs.unlock();
    return true;
}

void FileDescriptorManager::Delete() {
    while (true) {
        m_currentFDs.lock();
        AVLTree::wAVLTreeNode* node = m_currentFDs.FindNodeOrHigher(0);
        if (node == nullptr) {
            delete[] m_bitmap.GetBuffer();
            m_bitmap.SetBuffer(nullptr);
            m_bitmap.SetSize(0);
            m_currentFDs.unlock();
            return;
        }
        FDTableEntry* entry = reinterpret_cast<FDTableEntry*>(node->value);
        m_currentFDs.RemoveNode(node);
        m_currentFDs.unlock();

        if (entry != nullptr) {
            if (entry->desc != nullptr)
                entry->desc->Unref();
            delete entry;
        }
    }
}

fd_t FileDescriptorManager::Allocate(FileDescriptor* desc, int fdFlags, fd_t minFD) {
    if (desc == nullptr || minFD < 0 || minFD >= MAX_FD)
        return -EINVAL;

    FDTableEntry* entry = new FDTableEntry;
    if (entry == nullptr)
        return -ENOMEM;
    entry->desc = desc;
    entry->flags = fdFlags & FD_CLOEXEC;

    m_currentFDs.lock();
    fd_t fd = AllocateLocked(minFD);
    if (fd >= 0)
        m_currentFDs.Insert(fd, entry);
    m_currentFDs.unlock();

    if (fd < 0)
        delete entry;
    return fd;
}

bool FileDescriptorManager::Free(fd_t fd, FileDescriptor** descOut) {
    m_currentFDs.lock();

    AVLTree::wAVLTreeNode* node = m_currentFDs.FindNode(fd);
    if (node == nullptr) {
        m_currentFDs.unlock();
        return false;
    }

    FDTableEntry* entry = reinterpret_cast<FDTableEntry*>(node->value);
    m_currentFDs.RemoveNode(node);
    m_bitmap.Set(fd, false);
    m_currentFDs.unlock();

    if (descOut != nullptr)
        *descOut = entry->desc; // slot's reference moves to the caller
    else
        entry->desc->Unref();

    delete entry;
    return true;
}

fd_t FileDescriptorManager::Replace(fd_t fd, FileDescriptor* desc, int fdFlags) {
    if (desc == nullptr)
        return -EINVAL;
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    FDTableEntry* newEntry = new FDTableEntry;
    if (newEntry == nullptr)
        return -ENOMEM;

    FileDescriptor* old = nullptr;
    int rc = ESUCCESS;

    m_currentFDs.lock();
    FDTableEntry* existingEntry = m_currentFDs.Find(fd);
    if (existingEntry != nullptr) {
        old = existingEntry->desc;
        existingEntry->desc = desc;
        existingEntry->flags = fdFlags & FD_CLOEXEC;
    } else if (!GrowLocked((uint64_t)fd + 1))
        rc = -ENOMEM;
    else {
        newEntry->desc = desc;
        newEntry->flags = fdFlags & FD_CLOEXEC;
        m_bitmap.Set(fd, true);
        m_currentFDs.Insert(fd, newEntry);
        newEntry = nullptr; // now owned by the table
    }
    m_currentFDs.unlock();

    delete newEntry;
    if (rc < 0)
        return rc;

    if (old != nullptr)
        old->Unref();
    return fd;
}

bool FileDescriptorManager::ReserveFD(fd_t fd, FileDescriptor* desc, int fdFlags) {
    if (desc == nullptr || fd < 0 || fd >= MAX_FD)
        return false;

    FDTableEntry* entry = new FDTableEntry;
    if (entry == nullptr)
        return false;
    entry->desc = desc;
    entry->flags = fdFlags & FD_CLOEXEC;

    m_currentFDs.lock();
    if (!GrowLocked((uint64_t)fd + 1) || m_bitmap.Get(fd)) {
        m_currentFDs.unlock();
        delete entry;
        return false;
    }
    m_bitmap.Set(fd, true);
    m_currentFDs.Insert(fd, entry);
    m_currentFDs.unlock();

    return true;
}

FileDescriptor* FileDescriptorManager::Get(fd_t fd, int* fdFlags) {
    m_currentFDs.lock();
    FDTableEntry* entry = m_currentFDs.Find(fd);
    FileDescriptor* desc = nullptr;
    if (entry != nullptr) {
        desc = entry->desc;
        desc->Ref();
        if (fdFlags != nullptr)
            *fdFlags = entry->flags;
    }
    m_currentFDs.unlock();
    return desc;
}

int FileDescriptorManager::GetFDFlags(fd_t fd) {
    m_currentFDs.lock();
    FDTableEntry* entry = m_currentFDs.Find(fd);
    int flags = entry != nullptr ? entry->flags : -EBADF;
    m_currentFDs.unlock();
    return flags;
}

int FileDescriptorManager::SetFDFlags(fd_t fd, int flags) {
    m_currentFDs.lock();
    FDTableEntry* entry = m_currentFDs.Find(fd);
    int rc = -EBADF;
    if (entry != nullptr) {
        entry->flags = flags & FD_CLOEXEC;
        rc = ESUCCESS;
    }
    m_currentFDs.unlock();
    return rc;
}

bool FileDescriptorManager::Fork(FileDescriptorManager* other) {
    return CopyFrom(other, false);
}

bool FileDescriptorManager::Exec(FileDescriptorManager* other) {
    return CopyFrom(other, true);
}

bool FileDescriptorManager::CopyFrom(FileDescriptorManager* other, bool forExec) {
    if (other == nullptr || other == this)
        return false;

    Delete(); // ensure the table is empty

    bool ok = true;

    other->m_currentFDs.lock();
    m_currentFDs.lock();

    size_t size = other->m_bitmap.GetSize();
    if (size == 0)
        size = INITIAL_FD_TABLE_SIZE / 8;

    uint8_t* buffer = new uint8_t[size];
    if (buffer == nullptr)
        ok = false;
    else {
        memset(buffer, 0, size);
        m_bitmap.SetBuffer(buffer);
        m_bitmap.SetSize(size);

        other->m_currentFDs.forEach([&](fd_t fd, FDTableEntry* entry) -> bool {
            if (entry == nullptr || entry->desc == nullptr)
                return true;
            if (forExec && (entry->flags & FD_CLOEXEC) > 0)
                return true; // closed on exec

            FDTableEntry* copy = new FDTableEntry;
            if (copy == nullptr) {
                ok = false;
                return false;
            }
            copy->desc = entry->desc;
            copy->flags = entry->flags;
            copy->desc->Ref();

            m_bitmap.Set(fd, true);
            m_currentFDs.Insert(fd, copy);

            return true;
        });
    }

    m_currentFDs.unlock();
    other->m_currentFDs.unlock();

    if (!ok)
        Delete(); // release whatever was copied
    return ok;
}

bool FileDescriptorManager::GrowLocked(uint64_t bits) {
    uint64_t currentBits = m_bitmap.GetSize() * 8;
    if (bits <= currentBits)
        return true;
    if (bits > MAX_FD)
        return false;

    uint64_t newBits = ALIGN_UP(bits, INITIAL_FD_TABLE_SIZE);
    size_t oldSize = m_bitmap.GetSize();
    size_t newSize = newBits / 8;

    uint8_t* buffer = new uint8_t[newSize]; // new[] clears the buffer
    if (buffer == nullptr)
        return false;

    if (oldSize > 0)
        memcpy(buffer, m_bitmap.GetBuffer(), oldSize);

    delete[] m_bitmap.GetBuffer();
    m_bitmap.SetBuffer(buffer);
    m_bitmap.SetSize(newSize);
    return true;
}

fd_t FileDescriptorManager::AllocateLocked(fd_t minFD) {
    uint64_t bits = m_bitmap.GetSize() * 8;
    for (uint64_t i = minFD; i < bits; i++) {
        if (!m_bitmap.Get(i)) {
            m_bitmap.Set(i, true);
            return i;
        }
    }

    uint64_t first = (uint64_t)minFD > bits ? (uint64_t)minFD : bits;
    if (first >= MAX_FD)
        return -EMFILE;
    if (!GrowLocked(first + 1))
        return -ENOMEM;

    m_bitmap.Set(first, true);
    return first;
}
