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

    VFS::VFS() {

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


    VNode::VNode() {

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


    // Decrement refCount of a VNode, and delete it if refCount is 0.
    void UnrefVNode(VNode* node) {

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

    int VFS_LookupPath(const char* path, VNode** vnode, VFS** vfs, Credential cred) {
        if (path == nullptr || vnode == nullptr || vfs == nullptr)
            return -EINVAL;

        if (path[0] != '/')
            return -ENOSYS;

        char const* currentPath = &path[1];
        if (currentPath[0] == '\0') {
            *vfs = g_rootVFS;
            *vnode = g_rootVFS->GetRoot();
            return ESUCCESS;
        }

        VFS* currentVFS = g_rootVFS;
        VNode* currentVNode = g_rootVFS->GetRoot();
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
                assert(false);
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

    int VFS_CreateDir(const char* path, const char* name, Credential cred) {
        if (path == nullptr || name == nullptr)
            return -EINVAL;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cred);
        if (rc < 0)
            return rc;

        VNode* vnode = nullptr;
        switch (vfs->GetType()) {
        case FSType::TempFS:
            vnode = new TempFSVNode();
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

    int VFS_CreateFile(const char* path, const char* name, Credential cred) {
        if (path == nullptr || name == nullptr)
            return -EINVAL;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cred);
        if (rc < 0)
            return rc;

        VNode* vnode = nullptr;
        switch (vfs->GetType()) {
        case FSType::TempFS:
            vnode = new TempFSVNode();
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

    int VFS_Open(const char* path, VNode** out, Credential cred) {
        if (path == nullptr || out == nullptr)
            return -EACCES;

        VNode* vnode = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &vfs, cred);
        if (rc < 0)
            return rc;

        rc = vnode->Open(0, cred);
        if (rc < 0)
            return rc;

        *out = vnode;
        return ESUCCESS;
    }


}