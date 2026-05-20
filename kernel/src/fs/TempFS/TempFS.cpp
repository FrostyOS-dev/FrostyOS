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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <DataStructures/AVLTree.hpp>

#include <Memory/VMM.hpp>

#include "../VFS.hpp"

#define ROOT_DIR_MODE 0755


// TODO: Check cred & flags

namespace FS {

    TempFS::TempFS() {

    }

    TempFS::~TempFS() {

    }

    int TempFS::Mount(int flags, void* backing, Credential cred) {
        VAttr attr = {VType::DIR, ROOT_DIR_MODE, cred.euid, cred.egid, FSType::TempFS, -1, 0, 0, PAGE_SIZE, 0, 0, 0, 0};

        TempFSVNode* root = new TempFSVNode(this);
        int rc = root->Create(nullptr, nullptr, 0, &attr, cred);
        if (rc < 0) {
            delete root;
            return rc;
        }

        RefVNode(root);

        m_root = root;
        m_nodeCovered = nullptr;
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

    
    TempFSVNode::TempFSVNode(VFS* vfs) : VNode(vfs), m_name(nullptr), m_nameLen(0), m_blocks(), m_children() {

    }

    TempFSVNode::~TempFSVNode() {

    }

    int TempFSVNode::Open(int flags, Credential cred) {
        return ESUCCESS;
    }

    int TempFSVNode::Close(int flags, Credential cred) {
        return ESUCCESS;
    }

    int TempFSVNode::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, Credential cred) {
        if (offset >= m_attr.size)
            return -EINVAL;

        size_t read = 0;
        while (read < size) {
            if (offset + read >= m_attr.size)
                break;
            uint64_t blockNum = (offset + read) >> PAGE_SIZE_SHIFT;
            AVLTree::wAVLTreeNode* node = m_blocks.FindNodeOrLower(blockNum);
            if (node == nullptr || node->key + reinterpret_cast<Block*>(node->value)->pages <= blockNum) {
                // we are in the range of the file, this block just doesn't exist
                AVLTree::wAVLTreeNode* nextNode = m_blocks.FindNodeOrHigher(blockNum);
                if (nextNode == nullptr || nextNode->key >= offset + size) {
                    // just allocate a node from blockNum to end of requested region to read
                    Block* block = CreateBlock(blockNum, DIV_ROUNDUP(size - read, PAGE_SIZE));
                    if (block == nullptr) {
                        *bytesRead = read;
                        return -ENOMEM;
                    }

                    memcpy((void*)((uint64_t)out + read), block->addr, size - read);
                    read = size;
                    break;
                } else {
                    // need to allocate a block to fill the gap
                    Block* block = CreateBlock(blockNum, nextNode->key - blockNum);
                    if (block == nullptr) {
                        *bytesRead = read;
                        return -ENOMEM;
                    }

                    memcpy((void*)((uint64_t)out + read), block->addr, block->pages * PAGE_SIZE);
                    read += block->pages * PAGE_SIZE;

                    block = reinterpret_cast<Block*>(nextNode->value);
                    uint64_t readSize = MIN(size - read, block->pages * PAGE_SIZE);
                    memcpy((void*)((uint64_t)out + read), block->addr, readSize);
                    read += readSize;

                    if (read == size)
                        break;
                    else
                        continue;
                }
            }
            uint64_t actualBlockNum = node->key;
            Block* block = reinterpret_cast<Block*>(node->value);

            uint64_t readSize = MIN(size - read, (block->pages - (blockNum - actualBlockNum)) * PAGE_SIZE);
            memcpy((void*)((uint64_t)out + read), (void*)((uint64_t)block->addr + (blockNum - actualBlockNum) * PAGE_SIZE), readSize);
            read += readSize;
        }
        
        
        *bytesRead = read;
        return ESUCCESS;
    }

    int TempFSVNode::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, Credential cred) {
        size_t written = 0;
        while (written < size) {
            uint64_t blockNum = (offset + written) >> PAGE_SIZE_SHIFT;
            if (offset + written >= m_attr.size) {
                Block* block = CreateBlock(blockNum, DIV_ROUNDUP(size - written, PAGE_SIZE));
                if (block == nullptr) {
                    *bytesWritten = written;
                    return -ENOMEM;
                }

                memcpy(block->addr, (void*)((uint64_t)in + written), size - written);
                m_attr.size += size - written;
                written = size;
                break;
            }

            AVLTree::wAVLTreeNode* node = m_blocks.FindNodeOrLower(blockNum);
            if (node == nullptr || node->key + reinterpret_cast<Block*>(node->value)->pages <= blockNum) {
                // we are in the range of the file, this block just doesn't exist
                AVLTree::wAVLTreeNode* nextNode = m_blocks.FindNodeOrHigher(blockNum);
                if (nextNode == nullptr || nextNode->key >= offset + size) {
                    // just allocate a node from blockNum to end of requested region to read
                    Block* block = CreateBlock(blockNum, DIV_ROUNDUP(size - written, PAGE_SIZE));
                    if (block == nullptr) {
                        *bytesWritten = written;
                        return -ENOMEM;
                    }

                    memcpy(block->addr, (void*)((uint64_t)in + written), size - written);
                    written = size;
                    break;
                } else {
                    // need to allocate a block to fill the gap
                    Block* block = CreateBlock(blockNum, nextNode->key - blockNum);
                    if (block == nullptr) {
                        *bytesWritten = written;
                        return -ENOMEM;
                    }

                    memcpy(block->addr, (void*)((uint64_t)in + written), block->pages * PAGE_SIZE);
                    written += block->pages * PAGE_SIZE;

                    block = reinterpret_cast<Block*>(nextNode->value);
                    uint64_t writeSize = MIN(size - written, block->pages * PAGE_SIZE);
                    memcpy(block->addr, (void*)((uint64_t)in + written), writeSize);
                    written += writeSize;

                    if (written == size)
                        break;
                    else
                        continue;
                }
            }
            uint64_t actualBlockNum = node->key;
            Block* block = reinterpret_cast<Block*>(node->value);

            uint64_t writeSize = MIN(size - written, (block->pages - (blockNum - actualBlockNum)) * PAGE_SIZE);
            memcpy((void*)((uint64_t)block->addr + (blockNum - actualBlockNum) * PAGE_SIZE), (void*)((uint64_t)in + written), writeSize);
            written += writeSize;
        }
        
        
        *bytesWritten = written;
        return ESUCCESS;
    }

    int TempFSVNode::Lookup(const char* name, size_t nameLen, VNode** out, Credential cred) {
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

    int TempFSVNode::Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, Credential cred) {
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
        m_refCount = 1;

        m_refCount++;

        if (parent != nullptr) {
            TempFSVNode* fsVNode = static_cast<TempFSVNode*>(parent);
            fsVNode->m_children.lock();
            fsVNode->m_children.insert(this);
            fsVNode->m_children.unlock();
        }

        m_parent = parent;
        if (parent != nullptr)
            parent->GetRefCount()++;

        return ESUCCESS;
    }

    int TempFSVNode::GetAttr() {
        return -ENOSYS;
    }

    int TempFSVNode::SetAttr() {
        return -ENOSYS;
    }

    int TempFSVNode::Access() {
        return -ENOSYS;
    }

    int TempFSVNode::Link() {
        return -ENOSYS;
    }

    int TempFSVNode::Unlink() {
        return -ENOSYS;
    }

    int TempFSVNode::Symlink() {
        return -ENOSYS;
    }

    int TempFSVNode::ReadLink() {
        return -ENOSYS;
    }

    int TempFSVNode::Mmap() {
        return -ENOSYS;
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

    TempFSVNode::Block* TempFSVNode::CreateBlock(uint64_t offset, uint64_t pages) {
        Block* block = new Block;
        block->pages = pages;
        block->addr = VMM::g_KVMM->AllocatePages(pages);
        if (block->addr == nullptr) {
            delete block;
            return nullptr;
        }

        m_blocks.Insert(offset, block);
        return block;
    }

}