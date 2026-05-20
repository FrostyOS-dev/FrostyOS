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

#include <errno.h>
#include <string.h>

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

        VAttr attr = {VType::DIR, DEFAULT_DIR_MODE, cred.euid, cred.egid, vfs->GetType(), -1, 0, 0, 0, 0, 0, 0, 0};
        rc = vnode->Create(parent, name, strlen(name), &attr, cred);
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

        VAttr attr = {VType::REG, DEFAULT_FILE_MODE, cred.euid, cred.egid, vfs->GetType(), -1, 0, 0, 0, 0, 0, 0, 0};
        rc = vnode->Create(parent, name, strlen(name), &attr, cred);
        if (rc < 0) {
            delete vnode;
            return rc;
        }

        return ESUCCESS;
    }

    int VFS_Open(const char* path, VNode** out, VNode* cwd, Credential cred) {
        if (path == nullptr || out == nullptr)
            return -EACCES;

        VNode* vnode = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        rc = vnode->Open(0, cred);
        if (rc < 0)
            return rc;

        RefVNode(vnode);

        *out = vnode;
        return ESUCCESS;
    }

    int VFS_Close(VNode* vnode, Credential cred) {
        if (vnode == nullptr)
            return -EACCES;

        int rc = vnode->Close(0, cred);
        if (rc >= 0)
            UnrefVNode(vnode);
        return rc;
    }


}