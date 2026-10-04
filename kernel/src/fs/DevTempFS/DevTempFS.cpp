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

#include "DevTempFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <HAL/drivers/Device.hpp>
#include <HAL/drivers/DeviceManager.hpp>

#include "../VFS.hpp"

#define ROOT_DIR_MODE 0755

#define CALL_DEV_OP(op, ...) if (m_dev == nullptr) return -ENOTSUP; return m_dev->op(__VA_ARGS__)

namespace FS {
    DevTempFS::DevTempFS() {

    }

    DevTempFS::~DevTempFS() {

    }

    int DevTempFS::Mount(VNode* nodeCovered, int flags, void* backing, Credential cred) {
        VAttr attr = {VType::DIR, ROOT_DIR_MODE, cred.euid, cred.egid, FSType::DevTempFS, -1, 0, 0, PAGE_SIZE, {0, 0}, {0, 0}, {0, 0}, 0};

        DevTempFSVNode* root = new DevTempFSVNode(this, nullptr);
        int rc = root->Create(nullptr, nullptr, 0, &attr, cred);
        if (rc < 0) {
            delete root;
            return rc;
        }

        RefVNode(root);

        static_cast<DeviceManager*>(backing)->SetDevFS(this);

        m_root = root;
        m_nodeCovered = nodeCovered;
        m_next = nullptr;
        m_flags = 0;

        return ESUCCESS;
    }

    int DevTempFS::Unmount() {
        return -ENOSYS;
    }

    int DevTempFS::StatFS() {
        return -ENOSYS;
    }

    int DevTempFS::Sync() {
        return -ENOSYS;
    }

    FSType DevTempFS::GetType() {
        return FSType::DevTempFS;
    }

    
    DevTempFSVNode::DevTempFSVNode(VFS* vfs, Device* dev) : VNode(vfs), m_name(nullptr), m_nameLen(0), m_dev(dev), m_children() {

    }

    DevTempFSVNode::~DevTempFSVNode() {

    }

    int DevTempFSVNode::Open(int flags, Credential cred) {
        CALL_DEV_OP(Open, flags, cred);
    }

    int DevTempFSVNode::Close(int flags, Credential cred) {
        CALL_DEV_OP(Close, flags, cred);
    }

    int DevTempFSVNode::Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, Credential cred) {
        CALL_DEV_OP(Read, out, size, flags, offset, bytesRead, cred);
    }

    int DevTempFSVNode::Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, Credential cred) {
        CALL_DEV_OP(Write, in, size, flags, offset, bytesWritten, cred);
    }

    int DevTempFSVNode::Lookup(const char* name, size_t nameLen, VNode** out, Credential cred) {
        if (name == nullptr || nameLen == 0)
            return -EINVAL;

        struct SearchData {
            const char* name;
            size_t nameLen;
            DevTempFSVNode* node;
        } data = {name, nameLen, nullptr};

        m_children.lock();
        m_children.Enumerate([](DevTempFSVNode* child, void* data) -> bool {
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

    int DevTempFSVNode::Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, Credential cred) {
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
            DevTempFSVNode* fsVNode = static_cast<DevTempFSVNode*>(parent);
            fsVNode->m_children.lock();
            fsVNode->m_children.insert(this);
            fsVNode->m_children.unlock();
        }

        m_parent = parent;
        if (parent != nullptr)
            parent->GetRefCount()++;

        return ESUCCESS;
    }

    int DevTempFSVNode::GetAttr(VAttr* out) {
        if (out == nullptr)
            return -EINVAL;
        memcpy(out, &m_attr, sizeof(VAttr));
        return ESUCCESS;
    }

    int DevTempFSVNode::SetAttr(const VAttr& attr) {
        memcpy(&m_attr, &attr, sizeof(VAttr));
        return ESUCCESS;
    }

    int DevTempFSVNode::GetDents(Dentry* buffer, size_t count, uint64_t offset, size_t* readCount) {
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

        m_children.Enumerate([](DevTempFSVNode* vnode, uint64_t i, void* data) -> bool {
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

    int DevTempFSVNode::Access() {
        return -ENOSYS;
    }

    int DevTempFSVNode::Link() {
        return -ENOSYS;
    }

    int DevTempFSVNode::Unlink() {
        return -ENOSYS;
    }

    int DevTempFSVNode::Symlink(const char* path, size_t pathLen, Credential cred) {
        m_linkDest = new char[pathLen + 1];
        strncpy(m_linkDest, path, pathLen);
        m_linkDest[pathLen] = '\0';
        m_linkLen = pathLen;
        return ESUCCESS;
    }

    int DevTempFSVNode::ReadLink(char* buffer, size_t size, Credential cred) {
        if (size <= m_linkLen)
            return -ENAMETOOLONG;
        strncpy(buffer, m_linkDest, m_linkLen);
        buffer[m_linkLen] = '\0';
        return ESUCCESS;
    }

    int DevTempFSVNode::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, Credential cred) {
        CALL_DEV_OP(Mmap, offset, size, obj, cred);
    }

    int DevTempFSVNode::Munmap() {
        CALL_DEV_OP(Munmap);
    }

    int DevTempFSVNode::Resize() {
        return -ENOSYS;
    }

    int DevTempFSVNode::Rename() {
        return -ENOSYS;
    }

    int DevTempFSVNode::GetName(char* buf, size_t size, size_t* realSize) {
        if (size <= m_nameLen)
            return -ERANGE;
        memcpy(buf, m_name, m_nameLen);
        *realSize = m_nameLen;
        return ESUCCESS;
    }

    int DevTempFSVNode::Ioctl(size_t op, void* arg, int* result, Process* proc, Credential cred) {
        CALL_DEV_OP(Ioctl, op, arg, result, proc, cred);
    }

    bool DevTempFSVNode::HasChildren() {
        return m_children.getCount() != 0;
    }

    void DevTempFSVNode::SetDevice(Device* dev) {
        m_dev = dev;
    }

    Device* DevTempFSVNode::GetDevice() {
        return m_dev;
    }
}