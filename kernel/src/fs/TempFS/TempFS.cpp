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

#include "TempFS.hpp"
#include "TempFSPager.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <util.h>

#include <DataStructures/AVLTree.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>

#include <HAL/Time.hpp>

#include "../VFS.hpp"

#define ROOT_DIR_MODE 0755


namespace FS {

    TempFS::TempFS() : m_pager() {

    }

    TempFS::~TempFS() {

    }

    int TempFS::Mount(VNode* nodeCovered, int flags, void* backing, const Credential& cred) {
        time_t time = HAL_GetUnixEpochTime();
        VAttr attr = {VType::DIR, ROOT_DIR_MODE, cred.euid, cred.egid, FSType::TempFS, GetInodeNum(), 2, 0, PAGE_SIZE, {time, 0}, {time, 0}, {time, 0}, 0};

        TempFSVNode* root = new TempFSVNode(this);
        int rc = root->Create(nullptr, nullptr, 0, &attr, cred);
        if (rc < 0) {
            delete root;
            return rc;
        }

        RefVNode(root);

        m_root = root;
        m_nodeCovered = nodeCovered;
        m_next = nullptr;
        m_flags = 0;

        return ESUCCESS;
    }

    int TempFS::Unmount() {
        return -ENOSYS;
    }

    int TempFS::StatFS() {
        return -ENOSYS;
    }

    int TempFS::Sync() {
        return -ENOSYS;
    }

    FSType TempFS::GetType() {
        return FSType::TempFS;
    }

    TempFSPager* TempFS::GetPager() {
        return &m_pager;
    }

    
    TempFSVNode::TempFSVNode(VFS* vfs) : VNode(vfs), m_name(nullptr), m_nameLen(0), m_linkDest(nullptr), m_linkLen(0), m_memObj(nullptr), m_defaultProt(VMM::Protection::READ_WRITE), m_blocks(), m_children() {

    }

    TempFSVNode::~TempFSVNode() {
        assert(m_children.getCount() == 0 && "directory freed while it still has children");
        m_blocks.forEach([&](uint64_t, Block* block) -> bool {
            VMM::g_KVMM->FreePages(block->addr);
            delete block;
            return true;
        });
        m_blocks.Clear();
        delete[] m_name;
        delete[] m_linkDest;
        UnrefVNode(m_parent);
    }

    int TempFSVNode::Open(int flags, const Credential& cred) {
        return ESUCCESS;
    }

    int TempFSVNode::Close(int flags, const Credential& cred) {
        return ESUCCESS;
    }

    int TempFSVNode::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) {
        *bytesRead = 0;
        if (offset >= m_attr.size)
            return ESUCCESS;
        if (size > m_attr.size - offset)
            size = m_attr.size - offset;

        size_t done = 0;
        while (done < size) {
            uint64_t pos = offset + done;
            uint64_t pageOff = pos % PAGE_SIZE;
            size_t chunk = MIN((uint64_t)(size - done), (uint64_t)PAGE_SIZE - pageOff);
            void* page = GetPageAddr(pos >> PAGE_SIZE_SHIFT, false);
            if (page != nullptr)
                memcpy((uint8_t*)out + done, (uint8_t*)page + pageOff, chunk);
            else
                memset((uint8_t*)out + done, 0, chunk); // hole
            done += chunk;
        }
        *bytesRead = done;
        return ESUCCESS;
    }

    int TempFSVNode::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) {
        *bytesWritten = 0;
        if (offset + size < offset)
            return -EFBIG;

        int rc = ESUCCESS;
        size_t done = 0;
        while (done < size) {
            uint64_t pos = offset + done;
            uint64_t pageOff = pos % PAGE_SIZE;
            size_t chunk = MIN((uint64_t)(size - done), (uint64_t)PAGE_SIZE - pageOff);
            void* page = GetPageAddr(pos >> PAGE_SIZE_SHIFT, true);
            if (page == nullptr) {
                rc = -ENOMEM;
                break;
            }
            memcpy((uint8_t*)page + pageOff, (const uint8_t*)in + done, chunk);
            done += chunk;
        }

        if (offset + done > m_attr.size)
            m_attr.size = offset + done;
        *bytesWritten = done;
        return rc;
    }

    int TempFSVNode::Lookup(const char* name, size_t nameLen, VNode** out, const Credential& cred) {
        if (name == nullptr || nameLen == 0)
            return -EINVAL;

        struct SearchData {
            const char* name;
            size_t nameLen;
            TempFSVNode* node;
        } data = {name, nameLen, nullptr};

        m_children.lock();
        m_children.Enumerate([](TempFSVNode* child, void* data) -> bool {
            if (child == nullptr)
                return false;

            SearchData* d = static_cast<SearchData*>(data);
            if (child->m_nameLen == d->nameLen && 0 == memcmp(child->m_name, d->name, d->nameLen)) {
                d->node = child;
                return false;
            }

            return true;
        }, &data);
        m_children.unlock();

        if (data.node != nullptr) {
            *out = data.node;
            return ESUCCESS;
        }

        return -ENOENT;
    }

    int TempFSVNode::Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, const Credential& cred) {
        if ((parent != nullptr && (name == nullptr || nameLen == 0)) || attr == nullptr)
            return -EINVAL;

        VAttr tempAttr = *attr;
        tempAttr.size = 0;
        tempAttr.fsBlockSize = PAGE_SIZE;
        memcpy(&m_attr, &tempAttr, sizeof(VAttr));

        if (name != nullptr) {
            char* newName = new char[nameLen + 1];
            memcpy(newName, name, nameLen);
            newName[nameLen] = 0;
            m_name = newName;
            m_nameLen = nameLen;
        }

        m_vfsMounted = nullptr;
        m_refCount = 1; // directory entry's reference
        m_parent = parent;

        if (parent != nullptr) {
            TempFSVNode* fsVNode = static_cast<TempFSVNode*>(parent);
            fsVNode->m_children.lock();
            fsVNode->m_children.insert(this);
            fsVNode->m_children.unlock();
            RefVNode(parent);
        }

        return ESUCCESS;
    }

    int TempFSVNode::GetAttr(VAttr* out) {
        if (out == nullptr)
            return -EINVAL;
        memcpy(out, &m_attr, sizeof(VAttr));
        return ESUCCESS;
    }

    int TempFSVNode::SetAttr(const VAttr& attr) {
        memcpy(&m_attr, &attr, sizeof(VAttr));
        return ESUCCESS;
    }

    int TempFSVNode::GetDents(Dentry* buffer, size_t count, uint64_t offset, size_t* readCount) {
        struct Data {
            Dentry* buffer;
            size_t end;
            size_t read;
        } d = {buffer, offset + count, 0};
        
        m_children.lock();
        if (offset >= m_children.getCount()) {
            m_children.unlock();
            *readCount = 0;
            return ESUCCESS;
        }

        m_children.Enumerate([](TempFSVNode* vnode, uint64_t i, void* data) -> bool {
            Data* d = static_cast<Data*>(data);
            if (d->end == i)
                return false;

            vnode->Lock();

            d->buffer[d->read] = {vnode->m_attr.inode, static_cast<int64_t>(d->read), sizeof(Dentry), VFS_GetPosixType(vnode->m_attr.type), ""};
            memcpy(&d->buffer[d->read].name, vnode->m_name, vnode->m_nameLen);
            d->buffer[d->read].name[vnode->m_nameLen] = 0;

            vnode->Unlock();

            d->read++;

            return true;
        }, offset, &d);

        m_children.unlock();

        *readCount = d.read;
        return 0;
    }

    int TempFSVNode::Link() {
        return -ENOSYS;
    }

    int TempFSVNode::Unlink(VNode* child, const Credential& cred) {
        TempFSVNode* node = static_cast<TempFSVNode*>(child);
        if (node == nullptr || node->m_parent != this)
            return -ENOENT;

        m_children.lock();
        m_children.remove(node);
        m_children.unlock();

        node->m_attr.nlinks = 0;
        UnrefVNode(node);
        return ESUCCESS;
    }

    int TempFSVNode::Symlink(const char* path, size_t pathLen, const Credential& cred) {
        m_linkDest = new char[pathLen + 1];
        strncpy(m_linkDest, path, pathLen);
        m_linkDest[pathLen] = '\0';
        m_linkLen = pathLen;
        return ESUCCESS;
    }

    int TempFSVNode::ReadLink(char* buffer, size_t size, const Credential& cred) {
        if (size <= m_linkLen)
            return -ENAMETOOLONG;
        strncpy(buffer, m_linkDest, m_linkLen);
        buffer[m_linkLen] = '\0';
        return ESUCCESS;
    }

    int TempFSVNode::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
        if (offset + size > m_attr.blocks || obj == nullptr || size == 0 || (offset & (PAGE_SIZE - 1)) > 0 || (size & (PAGE_SIZE - 1)) > 0)
            return -EINVAL;

        if (m_memObj == nullptr) {
            m_blocks.lock();
            bool empty = m_blocks.isEmpty();
            m_blocks.unlock();
            if (empty)
                return -EINVAL;
            Block* b = CreateBlock(0, 1);
            if (b == nullptr || m_memObj == nullptr)
                return -ENOMEM;
            RefVNode(this); // memory object's reference
        }

        *obj = m_memObj;
        return ESUCCESS;
    }

    int TempFSVNode::Munmap() {
        return -ENOSYS;
    }

    int TempFSVNode::Resize() {
        return -ENOSYS;
    }

    int TempFSVNode::Rename() {
        return -ENOSYS;
    }

    int TempFSVNode::GetName(char* buf, size_t size, size_t* realSize) {
        if (size <= m_nameLen)
            return -ERANGE;
        memcpy(buf, m_name, m_nameLen);
        *realSize = m_nameLen;
        return ESUCCESS;
    }

    int TempFSVNode::Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) {
        return ENOSYS;
    }

    bool TempFSVNode::HasChildren() {
        return m_children.getCount() != 0;
    }

    void* TempFSVNode::GetAddr(uint64_t offset) {
        void* addr = GetPageAddr(offset >> PAGE_SIZE_SHIFT, false);
        return addr == nullptr ? nullptr : (void*)((uint64_t)addr + offset % PAGE_SIZE);
    }

    VMM::Protection TempFSVNode::GetDefaultProt() const {
        return m_defaultProt;
    }
    
    void* TempFSVNode::GetPageAddr(uint64_t pageIndex, bool create) {
        AVLTree::wAVLTreeNode* node = m_blocks.FindNodeOrLower(pageIndex);
        if (node != nullptr) {
            Block* b = reinterpret_cast<Block*>(node->value);
            if (pageIndex < node->key + b->pages)
                return (void*)((uint64_t)b->addr + (pageIndex - node->key) * PAGE_SIZE);
        }
        if (!create)
            return nullptr;
        Block* b = CreateBlock(pageIndex, 1);
        return b != nullptr ? b->addr : nullptr;
    }

    TempFSVNode::Block* TempFSVNode::CreateBlock(uint64_t offset, uint64_t pages) {
        Block* block = new Block;
        block->pages = pages;
        block->addr = VMM::g_KVMM->AllocMemObjAnonPages(pages, this, offset << PAGE_SIZE_SHIFT, VMM::DEFAULT_KALLOC_FLAGS, &m_memObj, static_cast<TempFS*>(m_vfs)->GetPager());
        if (block->addr == nullptr) {
            delete block;
            return nullptr;
        }

        m_defaultProt = VMM::Protection::READ_WRITE;
        m_blocks.Insert(offset, block);
        m_attr.blocks += pages * PAGE_SIZE;
        return block;
    }

}