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

#ifndef _FS_DEVTEMPFS_HPP
#define _FS_DEVTEMPFS_HPP

#include <stdint.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#include "../VFS.hpp"

class Device;

namespace FS {
    class DevTempFS : public VFS {
    public:
        DevTempFS();
        virtual ~DevTempFS() override;

        virtual int Mount(VNode* nodeCovered, int flags, void* backing, Credential cred) override;
        virtual int Unmount() override;
        virtual int StatFS() override;
        virtual int Sync() override;

        virtual FSType GetType() override;
    };

    class DevTempFSVNode : public VNode {
    public:
        DevTempFSVNode(VFS* vfs, Device* dev);
        virtual ~DevTempFSVNode() override;

        virtual int Open(int flags, Credential cred) override;
        virtual int Close(int flags, Credential cred) override;
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, Credential cred) override;
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, Credential cred) override;
        virtual int Lookup(const char* name, size_t nameLen, VNode** out, Credential cred) override;
        virtual int Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, Credential cred) override; // Verifying that a child vnode with the same name doesn't already exist is up to the caller.
        virtual int GetAttr(VAttr* out) override;
        virtual int SetAttr(const VAttr& attr) override;
        virtual int GetDents(Dentry* buffer, size_t count, uint64_t offset, size_t* readCount) override;
        virtual int Access() override;
        virtual int Link() override;
        virtual int Unlink() override;
        virtual int Symlink(const char* path, size_t pathLen, Credential cred) override;
        virtual int ReadLink(char* buffer, size_t size, Credential cred) override;
        virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, Credential cred) override;
        virtual int Munmap() override;
        virtual int Resize() override;
        virtual int Rename() override;
        virtual int GetName(char* buf, size_t size, size_t* realSize) override; // copy the null-terminated name into buf
        virtual int Ioctl(size_t op, void* arg, int* result, Process* proc, Credential cred) override;
        virtual bool HasChildren() override;

        void SetDevice(Device* dev);
        Device* GetDevice();

    private:
        char* m_name;
        size_t m_nameLen;

        char* m_linkDest;
        size_t m_linkLen;

        Device* m_dev;

        LinkedList::RearInsertLinkedList<DevTempFSVNode> m_children;
    };
}

#endif /* _FS_DEVTEMPFS_HPP */