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

#include "VFS.hpp"

#include "TempFS/TempFS.hpp"

#include <cstddef>
#include <errno.h>
#include <string.h>
#include <util.h>

#include <Memory/VMM.hpp>

#include <SystemCalls/Memory.hpp>

namespace FS {

    VFS* g_rootVFS = nullptr;

    VFS::VFS() : m_next(nullptr), m_nodeCovered(nullptr), m_root(nullptr), m_flags(0) {

    }

    VFS::~VFS() {

    }

    VFS* VFS::GetNext() {
        return m_next;
    }

    VNode* VFS::GetCoveredVNode() {
        return m_nodeCovered;
    }

    VNode* VFS::GetRoot() {
        return m_root;
    }


    VNode::VNode(VFS* vfs) : m_attr{VType::BAD, 0, 0, 0, FSType::Invalid, -1, 0, 0, 0, 0, 0, 0, 0}, m_lock(), m_refCount(0), m_vfs(vfs), m_vfsMounted(nullptr), m_parent(nullptr) {

    }

    VNode::~VNode() {

    }

    VFS* VNode::GetVFS() {
        return m_vfs;
    }

    VFS* VNode::GetMountedVFS() {
        return m_vfsMounted;
    }

    VType VNode::GetType() {
        return m_attr.type;
    }

    int& VNode::GetRefCount() {
        return m_refCount;
    }

    VNode* VNode::GetParent() {
        return m_parent;
    }

    void VNode::Lock() {
        m_lock.Lock();
    }

    void VNode::Unlock() {
        m_lock.Unlock();
    }

    // Increment refCount of a VNode, and delete it if refCount is 0.
    void RefVNode(VNode* node) {
        node->GetRefCount()++;
    }

    // Decrement refCount of a VNode, and delete it if refCount is 0.
    void UnrefVNode(VNode* node) {
        int& refCount = node->GetRefCount();
        refCount--;
        if (refCount == 0)
            delete node;
    }

    int VFS_Init() {
        g_rootVFS = nullptr;
        return ESUCCESS;
    }

    int VFS_MountRoot(FSType type, int flags, void* backing, Credential cred) {
        VFS* root = nullptr;
        switch (type) {
        case FSType::TempFS: {
            root = new TempFS();
            break;
        }
        }

        int rc = root->Mount(flags, backing, cred);
        if (rc < 0) {
            delete root;
            return rc;
        }

        g_rootVFS = root;
        return ESUCCESS;
    }

    int VFS_LookupPath(const char* path, VNode** vnode, VFS** vfs, VNode* cwd, Credential cred) {
        if (path == nullptr || vnode == nullptr || vfs == nullptr)
            return -EINVAL;

        VFS* currentVFS = g_rootVFS;
        VNode* currentVNode = g_rootVFS->GetRoot();
        char const* currentPath = path;

        if (currentPath[0] != '/' && currentPath[0] != '\0') {
            if (cwd == nullptr)
                return -EINVAL;
            if (VFS* mounted = cwd->GetMountedVFS(); mounted != nullptr) {
                currentVFS = mounted;
                currentVNode = currentVFS->GetRoot();
            } else {
                currentVFS = cwd->GetVFS();
                currentVNode = cwd;
            }
        } else if (currentPath[0] == '/')
            currentPath = &path[1];

        if (currentPath[0] == '\0') {
            *vfs = currentVFS;
            *vnode = currentVNode;
            return ESUCCESS;
        }

        char const* next = strchr(currentPath, '/');
        while (true) {
            if (currentVFS == nullptr || currentVNode == nullptr)
                return -ENOSYS;
            if (VFS* mounted = currentVNode->GetMountedVFS(); mounted != nullptr) {
                currentVFS = mounted;
                currentVNode = currentVFS->GetRoot();
            }
            if (currentPath[0] == '/')
                currentPath++;
            if (currentPath[0] == '\0')
                break;
            if (next == nullptr) {
                // last segment
                size_t len = strlen(currentPath);
                if (currentPath[len - 1] == '/')
                    break;
                if (currentVNode->GetType() != VType::DIR)
                    return -ENOTDIR;
                VNode* next = nullptr;
                int rc = currentVNode->Lookup(currentPath, len, &next, cred);
                if (rc < 0)
                    return rc;
                currentVNode = next;
                currentVFS = currentVNode->GetVFS();
                break;
            }
            size_t len = (size_t)(next - currentPath);
            if (len == 2 && strncmp(currentPath, "..", 2) == 0) {
                if (currentVNode->GetType() != VType::DIR)
                    return -ENOTDIR;
                VNode* parent = currentVNode->GetParent();
                if (parent == nullptr) { // must be the root of a VFS
                    parent = currentVFS->GetCoveredVNode();
                    if (parent == nullptr)
                        return -ENOENT; // must be at the root, can't go up any higher
                    currentVFS = parent->GetVFS();
                }
                currentVNode = parent;
            } else if (!(len == 1 && currentPath[0] == '.')) {
                if (currentVNode->GetType() != VType::DIR)
                    return -ENOTDIR;
                VNode* nextVNode = nullptr;
                int rc = currentVNode->Lookup(currentPath, len, &nextVNode, cred);
                if (rc < 0)
                    return rc;
                currentVNode = nextVNode;
            }

            currentPath = next;
            if (currentPath[0] == '/')
                currentPath++;
            next = strchr(currentPath, '/');
        }

        if (VFS* mounted = currentVNode->GetMountedVFS(); mounted != nullptr) {
            currentVFS = mounted;
            currentVNode = currentVFS->GetRoot();
        }

        *vnode = currentVNode;
        *vfs = currentVFS;

        return ESUCCESS;
    }

    int VFS_CreateDir(const char* path, const char* name, VNode* cwd, Credential cred) {
        if (path == nullptr || name == nullptr)
            return -EINVAL;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        VNode* vnode = nullptr;
        switch (vfs->GetType()) {
        case FSType::TempFS:
            vnode = new TempFSVNode(vfs);
            break;
        default:
            return -ENOSYS;
        }

        size_t nameLen = strlen(name);
        if (nameLen > NAME_MAX) {
            delete vnode;
            return -ENAMETOOLONG;
        }

        VAttr attr = {VType::DIR, DEFAULT_DIR_MODE, cred.euid, cred.egid, vfs->GetType(), -1, 0, 0, 0, 0, 0, 0, 0};
        rc = vnode->Create(parent, name, nameLen, &attr, cred);
        if (rc < 0) {
            delete vnode;
            return rc;
        }

        return ESUCCESS;
    }

    int VFS_CreateFile(const char* path, const char* name, VNode* cwd, Credential cred) {
        if (path == nullptr || name == nullptr)
            return -EINVAL;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        VNode* vnode = nullptr;
        switch (vfs->GetType()) {
        case FSType::TempFS:
            vnode = new TempFSVNode(vfs);
            break;
        default:
            return -ENOSYS;
        }

        size_t nameLen = strlen(name);
        if (nameLen > NAME_MAX) {
            delete vnode;
            return -ENAMETOOLONG;
        }
        
        VAttr attr = {VType::REG, DEFAULT_FILE_MODE, cred.euid, cred.egid, vfs->GetType(), -1, 0, 0, 0, 0, 0, 0, 0};
        rc = vnode->Create(parent, name, nameLen, &attr, cred);
        if (rc < 0) {
            delete vnode;
            return rc;
        }

        return ESUCCESS;
    }

    int VFS_Open(const char* path, VNode** out, VNode* cwd, Credential cred) {
        if (path == nullptr || out == nullptr)
            return -EINVAL;

        VNode* vnode = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        vnode->Lock();
        rc = vnode->Open(0, cred);
        vnode->Unlock();
        if (rc < 0)
            return rc;

        RefVNode(vnode);

        *out = vnode;
        return ESUCCESS;
    }

    int VFS_Close(VNode* vnode, Credential cred) {
        if (vnode == nullptr)
            return -EINVAL;

        vnode->Lock();
        int rc = vnode->Close(0, cred);
        vnode->Unlock();
        if (rc >= 0)
            UnrefVNode(vnode);
        return rc;
    }

    int VFS_MapFile(void* hint, size_t length, VMM::Protection prot, int flags, bool user, VNode* vnode, uint64_t offset, void** addr, VMM::VMM* vmm, const Credential& cred) {
        if (vmm == nullptr || vnode == nullptr || addr == nullptr)
            return -EINVAL;

        VMM::AllocFlags allocFlags = VMM::DEFAULT_ALLOC_FLAGS;
        allocFlags.protection = prot;
        allocFlags.user = user;
        allocFlags.isPrivate = (flags & MAP_PRIVATE) > 0;
        allocFlags.allocPhys = (flags & MAP_POPULATE) > 0;
        allocFlags.addrIsHint = (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) == 0;
        allocFlags.replace = (flags & MAP_FIXED) > 0;

        VMM::MemoryObject* obj;

        int rc = vnode->Mmap(offset, length, &obj, cred);
        if (rc < 0)
            return rc;

        void* pages = vmm->AllocateBackedPages(length >> PAGE_SIZE_SHIFT, obj, offset, hint, allocFlags);
        if (pages == nullptr)
            return -ENOMEM;

        *addr = pages;
        return ESUCCESS;
    }

    int VFS_BuildPath(VNode* vnode, char* buf, size_t size, Credential cred) {
        if (vnode == nullptr || buf == nullptr || size == 0)
            return -EINVAL;

        if (g_rootVFS == nullptr || g_rootVFS->GetRoot() == nullptr)
            return -ENOSYS;

        if (vnode == g_rootVFS->GetRoot()) { // Fast path: the VNode is the absolute root
            if (size < 2)
                return -ERANGE;
            buf[0] = '/';
            buf[1] = '\0';
            return ESUCCESS;
        }

        // Start writing from the end of the buffer
        char* ptr = buf + size - 1;
        *ptr = '\0';

        VNode* current = vnode;

        while (current != g_rootVFS->GetRoot()) {
            char nameBuf[NAME_MAX + 1];
            size_t nameLen = 0;

            int rc = current->GetName(nameBuf, NAME_MAX + 1, &nameLen);
            if (rc < 0)
                return rc;

            if (ptr - buf < static_cast<ptrdiff_t>(nameLen + 1))
                return -ERANGE;

            ptr -= nameLen;
            memcpy(ptr, nameBuf, nameLen);

            ptr--;
            *ptr = '/';

            // Move up the tree
            VNode* parent = current->GetParent();

            if (parent == nullptr) { // No parent, maybe root of a mounted VFS?
                VFS* vfs = current->GetVFS();
                if (vfs != nullptr)
                    parent = vfs->GetCoveredVNode();

                if (parent == nullptr) // still no parent, disconnected vnode
                    return -ENOENT;
            }

            current = parent;
        }

        // Shift the completely built path to the start of buf
        size_t pathLen = static_cast<size_t>((buf + size - 1) - ptr);
        memmove(buf, ptr, pathLen + 1);

        return ESUCCESS;
    }

    uint8_t VFS_GetPosixType(VType type) {
        switch (type) {
#define TYPE_CASE(v, p) \
        case VType::v: \
            return DT_##p

            TYPE_CASE(NON, UNKNOWN);
            TYPE_CASE(REG, REG);
            TYPE_CASE(DIR, DIR);
            TYPE_CASE(BLK, BLK);
            TYPE_CASE(CHR, CHR);
            TYPE_CASE(LNK, LNK);
            TYPE_CASE(SOCK, SOCK);
            TYPE_CASE(FIFO, FIFO);
            TYPE_CASE(BAD, UNKNOWN);
#undef TYPE_CASE
        }
        return DT_UNKNOWN;
    }


}