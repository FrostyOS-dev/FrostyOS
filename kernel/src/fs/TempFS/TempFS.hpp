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

#ifndef _TEMPFS_HPP
#define _TEMPFS_HPP

#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <Scheduling/Process.hpp>

#include "../VFS.hpp"

namespace FS {
    class TempFS : public VFS {
    public:
        TempFS();
        virtual ~TempFS() override;

        virtual int Mount(int flags, void* backing, Credential cred) override;
        virtual int Unmount() override;
        virtual int StatFS() override;
        virtual int Sync() override;

        virtual FSType GetType() override;
    };


    class TempFSVNode : public VNode {
    public:
        TempFSVNode(VFS* vfs);
        virtual ~TempFSVNode() override;

        virtual int Open(int flags, Credential cred) override;
        virtual int Close(int flags, Credential cred) override;
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, Credential cred) override;
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, Credential cred) override;
        virtual int Lookup(const char* name, size_t nameLen, VNode** out, Credential cred) override;
        virtual int Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, Credential cred) override; // Verifying that a child vnode with the same name doesn't already exist is up to the caller.
        virtual int GetAttr() override;
        virtual int SetAttr() override;
        virtual int Access() override;
        virtual int Link() override;
        virtual int Unlink() override;
        virtual int Symlink() override;
        virtual int ReadLink() override;
        virtual int Mmap() override;
        virtual int Munmap() override;
        virtual int Resize() override;
        virtual int Rename() override;

    private:
        struct Block {
            void* addr;
            size_t pages;
        };

        Block* CreateBlock(uint64_t offset, uint64_t pages); // offset and pages are page count numbers, not bytes

        char* m_name;
        size_t m_nameLen;

        AVLTree::wAVLTree<uint64_t, Block*> m_blocks;
        LinkedList::RearInsertLinkedList<TempFSVNode> m_children;
    };
}

#endif /* _TEMPFS_HPP */