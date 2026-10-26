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

#include "DevTempFS/DevTempFS.hpp"

#include "TempFS/TempFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <util.h>

#include <HAL/Time.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#include <SystemCalls/File.hpp>
#include <SystemCalls/Memory.hpp>

#define LINK_MAX 64

namespace FS {

    VFS* g_rootVFS = nullptr;
    VFS* g_VFSList = nullptr;
    Mutex g_listLock;

    dev_t g_NextDevNum = 1;

    static Mutex g_nsLock;

    struct VNodeRef {
        VNode* node;
        explicit VNodeRef(VNode* n) : node(n) {}
        ~VNodeRef() { UnrefVNode(node); }
        VNodeRef(const VNodeRef&) = delete;
        VNodeRef& operator=(const VNodeRef&) = delete;
    };

    struct NSLockGuard {
        NSLockGuard() { g_nsLock.Lock(); }
        ~NSLockGuard() { g_nsLock.Unlock(); }
    };

    static dev_t GetDevNum() {
        return __atomic_fetch_add(&g_NextDevNum, 1, __ATOMIC_RELAXED);
    }

    static bool CheckPermission(const VAttr& a, int want, const Credential& c) {
        want &= 7;

        if (c.euid == 0) // root bypasses r/w, but needs at least one x bit execute a non-directory
            return !((want & ACCESS_X) > 0 && a.type != VType::DIR && (a.mode & 0111) == 0);

        int granted;
        if (c.euid == a.uid)
            granted = (a.mode >> 6) & 7;
        else if (c.egid == a.gid)
            granted = (a.mode >> 3) & 7;
        else
            granted = a.mode & 7;

        return (want & ~granted) == 0;
    }

    static int CheckCreate(VNode* dir, const Credential& cred) {
        if (dir->GetType() != VType::DIR)
            return -ENOTDIR;
        return dir->Access(ACCESS_W | ACCESS_X, cred);
    }

    static void InheritFromParent(VAttr& attr, VNode* parent) {
        VAttr p;
        if (parent->GetAttr(&p) < 0 || (p.mode & S_ISGID) == 0)
            return;
        attr.gid = p.gid;
        if (attr.type == VType::DIR)
            attr.mode |= S_ISGID;
    }

    VFS::VFS() : m_next(nullptr), m_nodeCovered(nullptr), m_root(nullptr), m_flags(0), m_nextInodeNum(1), m_devNum(0) {

    }

    VFS::~VFS() {

    }

    VFS* VFS::GetNext() {
        return m_next;
    }

    void VFS::SetNext(VFS* vfs) {
        m_next = vfs;
    }

    VNode* VFS::GetCoveredVNode() {
        return m_nodeCovered;
    }

    VNode* VFS::GetRoot() {
        return m_root;
    }

    ino_t VFS::GetInodeNum() {
        return __atomic_fetch_add(&m_nextInodeNum, 1, __ATOMIC_RELAXED);
    }

    dev_t VFS::GetDevNum() const {
        return m_devNum;
    }

    void VFS::SetDevNum(dev_t dev) {
        m_devNum = dev;
    }

    void VFS::Lock() {
        m_lock.Lock();
    }

    void VFS::Unlock() {
        m_lock.Unlock();
    }


    VNode::VNode(VFS* vfs) : m_attr{VType::BAD, 0, 0, 0, FSType::Invalid, -1, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0}, m_lock(), m_refCount(0), m_vfs(vfs), m_vfsMounted(nullptr), m_parent(nullptr), m_unlinked(false) {

    }

    VNode::~VNode() {

    }

    int VNode::Access(int mode, const Credential& cred) {
        Lock();
        VAttr snapshot = m_attr;
        Unlock();
        return CheckPermission(snapshot, mode, cred) ? ESUCCESS : -EACCES;
    }

    int VNode::Isatty() {
        return -ENOTTY;
    }

    bool VNode::IsUnlinked() {
        return m_unlinked;
    }

    void VNode::SetUnlinked(bool unlinked) {
        m_unlinked = unlinked;
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

    uint64_t& VNode::GetRefCount() {
        return m_refCount;
    }

    VNode* VNode::GetParent() {
        return m_parent;
    }

    void VNode::SetMountedVFS(VFS* vfs) {
        m_vfsMounted = vfs;
    }

    EventWaitQueue* VNode::GetPollQueue(uint32_t events) {
        return nullptr;
    }

    uint32_t VNode::PollReady(uint32_t events) {
        return events & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
    }

    void VNode::Lock() {
        m_lock.Lock();
    }

    void VNode::Unlock() {
        m_lock.Unlock();
    }

    // Increment refCount of a VNode
    void RefVNode(VNode* node) {
        if (node != nullptr)
            __atomic_fetch_add(&node->GetRefCount(), 1, __ATOMIC_RELAXED);
    }

    // Decrement refCount of a VNode, and delete it if refCount is 0.
    void UnrefVNode(VNode* node) {
        if (node == nullptr)
            return;
        uint64_t n = __atomic_sub_fetch(&node->GetRefCount(), 1, __ATOMIC_ACQ_REL);
        assert(n != UINT64_MAX && "vnode refcount underflow");
        if (n == 0)
            delete node;
    }

    int VFS_Init() {
        g_rootVFS = nullptr;
        return ESUCCESS;
    }

    int VFS_MountRoot(FSType type, int flags, void* backing, const Credential& cred) {
        VFS* root = nullptr;
        switch (type) {
        case FSType::TempFS: {
            root = new TempFS();
            if (root == nullptr)
                return -ENOMEM;
            break;
        }
        default:
            return -EINVAL;
        }

        root->SetDevNum(GetDevNum());
        int rc = root->Mount(nullptr, flags, backing, cred);
        if (rc < 0) {
            delete root;
            return rc;
        }

        g_rootVFS = root;
        g_VFSList = root;
        return ESUCCESS;
    }

    int VFS_Mount(FSType type, const char* path, int flags, void* backing, VNode* cwd, const Credential& cred) {
        if (path == nullptr)
            return -EINVAL;

        if (cred.euid != 0)
            return -EACCES;

        VNode* vnode = nullptr;
        VFS* parentVFS = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &parentVFS, cwd, cred);
        if (rc < 0)
            return rc;

        vnode->Lock();

        VAttr attr;
        rc = vnode->GetAttr(&attr);
        if (rc < 0 || attr.type != VType::DIR || vnode->HasChildren()) {
            vnode->Unlock();
            UnrefVNode(vnode);
            return rc < 0 ? rc : (attr.type != VType::DIR ? -ENOTDIR : -ENOTEMPTY);
        }

        VFS* vfs = nullptr;
        switch (type) {
        case FSType::TempFS: {
            vfs = new TempFS();
            break;
        }
        case FSType::DevTempFS: {
            vfs = new DevTempFS();
            break;
        }
        default:
            vnode->Unlock();
            UnrefVNode(vnode);
            return -EINVAL;
        }

        if (vfs == nullptr) {
            vnode->Unlock();
            UnrefVNode(vnode);
            return -ENOMEM;
        }

        vfs->SetDevNum(GetDevNum());
        vfs->Lock();
        rc = vfs->Mount(vnode, flags, backing, cred);
        if (rc < 0) {
            vfs->Unlock();
            vnode->Unlock();
            UnrefVNode(vnode);
            delete vfs;
            return rc;
        }

        g_listLock.Lock();
        vfs->SetNext(g_VFSList);
        g_VFSList = vfs;
        g_listLock.Unlock();

        vfs->Unlock();

        vnode->SetMountedVFS(vfs);
        vnode->Unlock();

        return 0;
    }

    static int LookupPathLocked(const char* path, VNode** vnode, VFS** vfs, VNode* cwd, const Credential& cred, bool followSymlink) {
        if (path == nullptr || vnode == nullptr || vfs == nullptr)
            return -EINVAL;

        if (g_rootVFS == nullptr || g_rootVFS->GetRoot() == nullptr)
            return -ENOSYS;

        size_t pathLen = strlen(path);
        if (pathLen > PATH_MAX)
            return -ENAMETOOLONG;

        char pathBuffer[PATH_MAX + 1];
        memcpy(pathBuffer, path, pathLen + 1);

        VNode* current;
        if (pathBuffer[0] == '/')
            current = g_rootVFS->GetRoot();
        else {
            if (cwd == nullptr)
                return -EINVAL;
            current = cwd;
        }

        const char* rest = pathBuffer;
        int symlinkDepth = 0;
        bool mustBeDir = false;

        while (true) {
            while (*rest == '/')
                rest++;
            if (*rest == '\0')
                break;

            size_t len = 0;
            while (rest[len] != '\0' && rest[len] != '/')
                len++;
            if (len > NAME_MAX)
                return -ENAMETOOLONG;

            const char* after = rest + len;
            while (*after == '/')
                after++;

            bool last = (*after == '\0');
            bool trailingSlash = last && rest[len] == '/';
            mustBeDir = trailingSlash;

            if (current->GetType() != VType::DIR)
                return -ENOTDIR;
            if (int rc = current->Access(ACCESS_X, cred); rc < 0)
                return rc;

            if (len == 1 && rest[0] == '.') {
                rest = after;
                continue;
            }

            if (len == 2 && rest[0] == '.' && rest[1] == '.') {
                if (current == current->GetVFS()->GetRoot()) {
                    VNode* covered = current->GetVFS()->GetCoveredVNode();
                    if (covered == nullptr) { // global root: can't go any higher up
                        rest = after;
                        continue;
                    }
                    current = covered;
                }
                VNode* parent = current->GetParent();
                if (parent == nullptr)
                    return -ENOENT;
                current = parent;
                rest = after;
                continue;
            }

            VNode* next = nullptr;
            if (int rc = current->Lookup(rest, len, &next, cred); rc < 0)
                return rc;

            while (VFS* mounted = next->GetMountedVFS())
                next = mounted->GetRoot();

            bool follow = !last || followSymlink || trailingSlash;
            if (next->GetType() == VType::LNK && follow) {
                if (++symlinkDepth > LINK_MAX)
                    return -ELOOP;

                char target[PATH_MAX + 1] = {};
                if (int rc = next->ReadLink(target, PATH_MAX, cred); rc < 0)
                    return rc;

                size_t targetLen = strlen(target);
                if (targetLen == 0)
                    return -ENOENT;

                size_t remLen = strlen(after);
                bool keepSlash = trailingSlash && remLen == 0;
                size_t prefixLen = targetLen + ((remLen > 0 || keepSlash) ? 1 : 0);
                if (prefixLen + remLen > PATH_MAX)
                    return -ENAMETOOLONG;

                memmove(pathBuffer + prefixLen, after, remLen + 1);
                memcpy(pathBuffer, target, targetLen);
                if (remLen > 0 || keepSlash)
                    pathBuffer[targetLen] = '/';

                rest = pathBuffer;
                if (target[0] == '/')
                    current = g_rootVFS->GetRoot();
                continue;
            }

            current = next;
            rest = after;
        }

        if (mustBeDir && current->GetType() != VType::DIR)
            return -ENOTDIR;

        *vnode = current;
        *vfs = current->GetVFS();
        return ESUCCESS;
    }

    int VFS_LookupPath(const char *path, VNode** vnode, VFS** vfs, VNode* cwd, const Credential& cred, bool followSymlink) {
        g_nsLock.Lock();
        int rc = LookupPathLocked(path, vnode, vfs, cwd, cred, followSymlink);
        if (rc == ESUCCESS)
            RefVNode(*vnode);
        g_nsLock.Unlock();
        return rc;
    }

    static bool InGroup(gid_t gid, const Credential& c) {
        return gid == c.egid; // future proofing for suplementary groups
    }

    int VFS_Create(const char* path, const char* name, VType type, mode_t mode, VNode* cwd, const Credential& cred, VNode* vnode, VNode** outVNode, const char* dest) {
        if (path == nullptr || name == nullptr || (type == VType::LNK && dest == nullptr))
            return -EINVAL;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        VNodeRef parentRef(parent);

        rc = CheckCreate(parent, cred);
        if (rc < 0)
            return rc;

        size_t nameLen = strlen(name);
        if (nameLen > NAME_MAX)
            return -ENAMETOOLONG;

        if ((nameLen == 1 && name[0] == '.') || (nameLen == 2 && name[0] == '.' && name[1] == '.'))
            return -EEXIST;

        VNode* temp;
        parent->Lock();
        if (parent->IsUnlinked()) {
            parent->Unlock();
            return -ENOENT;
        }

        if (parent->Lookup(name, nameLen, &temp, cred) == 0) {
            parent->Unlock();
            return -EEXIST;
        }

        bool createdVNode = false;

        if (vnode == nullptr) {
            switch (vfs->GetType()) {
            case FSType::TempFS:
                vnode = new TempFSVNode(vfs);
                break;
            case FSType::DevTempFS:
                vnode = new DevTempFSVNode(vfs, nullptr);
                break;
            default:
                parent->Unlock();
                return -ENOSYS;
            }
            createdVNode = true;
        }

        if (vnode == nullptr) {
            parent->Unlock();
            return -ENOMEM;
        }

        time_t time = HAL_GetUnixEpochTime();
        VAttr attr = {type, (uint16_t)mode, cred.euid, cred.egid, vfs->GetType(), vfs->GetInodeNum(), type == VType::DIR ? 2 : 1, 0, 0, {time, 0}, {time, 0}, {time, 0}, 0};
        InheritFromParent(attr, parent);
        if (type != VType::DIR && type != VType::LNK && cred.euid != 0 && !InGroup(attr.gid, cred))
            attr.mode &= (uint16_t)~S_ISGID;
        rc = vnode->Create(parent, name, nameLen, &attr, cred);

        if (type == VType::LNK) {
            if (rc < 0) {
                parent->Unlock();
                if (createdVNode)
                    delete vnode;
                return rc;
            }
            rc = vnode->Symlink(dest, strlen(dest), cred);
        }

        parent->Unlock();
        if (rc < 0) {
            if (createdVNode)
                delete vnode;
            return rc;
        }

        if (outVNode != nullptr)
            *outVNode = vnode;

        return ESUCCESS;
    }

    int VFS_Open(const char* path, int flags, VNode** out, VNode* cwd, const Credential& cred) {
        if (path == nullptr || out == nullptr)
            return -EINVAL;

        VNode* vnode = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        VNodeRef ref(vnode);

        int want = 0;
        switch (flags & O_ACCMODE) {
        case O_RDONLY:
            want = ACCESS_R;
            break;
        case O_WRONLY:
            want = ACCESS_W;
            break;
        case O_RDWR:
            want = ACCESS_R | ACCESS_W;
            break;
        default:
            return -EINVAL;
        }
        if ((flags & O_TRUNC) > 0)
            want |= ACCESS_W;

        if (vnode->GetType() == VType::DIR && (want & ACCESS_W) > 0)
            return -EISDIR;

        rc = vnode->Access(want, cred);
        if (rc < 0)
            return rc;

        vnode->Lock();
        rc = vnode->Open(flags, cred);
        vnode->Unlock();
        if (rc < 0)
            return rc;

        RefVNode(vnode);

        *out = vnode;
        return ESUCCESS;
    }

    int VFS_OpenExec(const char* path, VNode** out, VAttr* outAttr, VNode* cwd, const Credential& cred) {
        if (path == nullptr || out == nullptr)
            return -EINVAL;

        VNode* vnode = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &vnode, &vfs, cwd, cred);
        if (rc < 0)
            return rc;

        VNodeRef ref(vnode);

        vnode->Lock();
        VAttr attr;
        rc = vnode->GetAttr(&attr);
        if (rc >= 0 && (attr.type != VType::REG || !CheckPermission(attr, ACCESS_X, cred)))
            rc = -EACCES;
        if (rc >= 0)
            rc = vnode->Open(O_RDONLY, cred); // kernel does the reading, caller only needs execute
        vnode->Unlock();
        if (rc < 0)
            return rc;

        RefVNode(vnode);
        *out = vnode;
        if (outAttr != nullptr)
            *outAttr = attr;
        return ESUCCESS;
    }

    int VFS_Close(VNode* vnode, const Credential& cred) {
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

    int VFS_BuildPath(VNode* vnode, char* buf, size_t size, const Credential& cred) {
        if (vnode == nullptr || buf == nullptr || size == 0)
            return -EINVAL;

        NSLockGuard ns;
        if (vnode->IsUnlinked())
            return -ENOENT;

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

    int VFS_Chmod(VNode* vnode, mode_t mode, const Credential& cred) {
        if (vnode == nullptr)
            return -EINVAL;

        vnode->Lock();

        VAttr attr;
        int rc = vnode->GetAttr(&attr);
        if (rc < 0) {
            vnode->Unlock();
            return rc;
        }

        if (cred.euid != 0 && cred.euid != attr.uid) {
            vnode->Unlock();
            return -EPERM;
        }

        uint16_t newMode = mode & S_PERMMASK;
        if (cred.euid != 0 && !InGroup(attr.gid, cred))
            newMode &= (uint16_t)~S_ISGID;

        attr.mode = newMode;
        attr.ctime.tv_sec = HAL_GetUnixEpochTime();
        attr.ctime.tv_nsec = 0;

        rc = vnode->SetAttr(attr);
        vnode->Unlock();
        return rc;
    }

    int VFS_Chown(VNode* vnode, uid_t uid, gid_t gid, const Credential& cred) {
        if (vnode == nullptr)
            return -EINVAL;

        vnode->Lock();

        VAttr attr;
        int rc = vnode->GetAttr(&attr);
        if (rc < 0) {
            vnode->Unlock();
            return rc;
        }

        if (cred.euid != 0) {
            bool owner = cred.euid == attr.uid;
            if (uid != -1 && (!owner || uid != attr.uid)) {
                vnode->Unlock();
                return -EPERM;
            }
            if (gid != -1 && (!owner || (gid != attr.gid && !InGroup(gid, cred)))) {
                vnode->Unlock();
                return -EPERM;
            }
        }

        if (uid != -1)
            attr.uid = uid;
        if (gid != -1)
            attr.gid = gid;

        if (attr.type != VType::DIR) {
            attr.mode &= (uint16_t)~S_ISUID;
            if ((attr.mode & 0010) > 0)
                attr.mode &= (uint16_t)~S_ISGID;
        }

        attr.ctime.tv_sec = HAL_GetUnixEpochTime();
        attr.ctime.tv_nsec = 0;

        rc = vnode->SetAttr(attr);
        vnode->Unlock();
        return rc;
    }

    void VFS_ClearSetID(VNode* vnode, const Credential& cred) {
        if (vnode == nullptr || cred.euid == 0)
            return;

        VAttr attr;
        if (vnode->GetAttr(&attr) < 0)
            return;

        uint16_t kill = attr.mode & (S_ISUID | S_ISGID);
        if (kill == 0)
            return;

        attr.mode &= (uint16_t)~kill;
        attr.ctime.tv_sec = HAL_GetUnixEpochTime();
        attr.ctime.tv_nsec = 0;
        vnode->SetAttr(attr);
    }

    static int UnlinkLocked(VNode* parent, const char* name, size_t nameLen, bool removeDir, bool trailingSlash, const Credential& cred, VNode** outChild) {
        if (parent->IsUnlinked())
            return -ENOENT;

        VNode* child = nullptr;
        int rc = parent->Lookup(name, nameLen, &child, cred);
        if (rc < 0)
            return rc;

        if (child->GetMountedVFS() != nullptr)
            return -EBUSY;

        VAttr pattr;
        parent->GetAttr(&pattr);

        child->Lock();
        VAttr cattr;
        child->GetAttr(&cattr);

        if (cattr.type == VType::DIR) {
            if (!removeDir) {
                child->Unlock();
                return -EISDIR;
            }
        } else if (removeDir || trailingSlash) {
            child->Unlock();
            return -ENOTDIR;
        }

        if ((pattr.mode & S_ISVTX) > 0 && cred.euid != 0 && cred.euid != cattr.uid && cred.euid != pattr.uid) {
            child->Unlock();
            return -EPERM;
        }

        if (cattr.type == VType::DIR && child->HasChildren()) {
            child->Unlock();
            return -ENOTEMPTY;
        }

        RefVNode(child);
        child->SetUnlinked(true);
        rc = parent->Unlink(child, cred);
        if (rc < 0) {
            child->SetUnlinked(false);
            child->Unlock();
            UnrefVNode(child);
            return rc;
        }
        child->Unlock();
        *outChild = child;
        return ESUCCESS;
    }

    int VFS_Unlink(const char* path, const char* name, VNode* cwd, const Credential& cred, bool removeDir, bool trailingSlash) {
        if (path == nullptr || name == nullptr)
            return -EINVAL;

        size_t nameLen = strlen(name);
        if (nameLen == 0)
            return -ENOENT;
        if (nameLen > NAME_MAX)
            return -ENAMETOOLONG;
        if (nameLen == 1 && name[0] == '.')
            return removeDir ? -EINVAL : -EISDIR;
        if (nameLen == 2 && name[0] == '.' && name[1] == '.')
            return removeDir ? -ENOTEMPTY : -EISDIR;

        VNode* parent = nullptr;
        VFS* vfs = nullptr;
        int rc = VFS_LookupPath(path, &parent, &vfs, cwd, cred);
        if (rc < 0)
            return rc;
        VNodeRef parentRef(parent);

        rc = CheckCreate(parent, cred);
        if (rc < 0)
            return rc;

        VNode* removed = nullptr;
        g_nsLock.Lock();
        parent->Lock();
        rc = UnlinkLocked(parent, name, nameLen, removeDir, trailingSlash, cred, &removed);
        parent->Unlock();
        g_nsLock.Unlock();
        if (rc < 0)
            return rc;

        UnrefVNode(removed);
        return ESUCCESS;
    }

    int VFS_Truncate(VNode* vnode, uint64_t size, const Credential& cred, bool clearSID) {
        if (vnode == nullptr)
            return -EINVAL;
    
        vnode->Lock();
        if (VType type = vnode->GetType(); type != VType::REG) {
            vnode->Unlock();
            return type == VType::DIR ? -EISDIR : -EPERM;
        }

        int rc = vnode->Resize(size, cred);
        if (rc >= 0) {
            VAttr attr;
            rc = vnode->GetAttr(&attr);
            if (rc >= 0) {
                if (cred.euid != 0 && clearSID)
                    attr.mode &= (uint16_t)~(S_ISUID | S_ISGID);
                attr.mtime.tv_sec = HAL_GetUnixEpochTime();
                attr.mtime.tv_nsec = 0;
                attr.ctime = attr.mtime;
                rc = vnode->SetAttr(attr);
            }
        }

        vnode->Unlock();
        return rc;
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

    static bool StickyDenies(const VAttr& dir, const VAttr& child, const Credential& c) {
        return (dir.mode & S_ISVTX) > 0 && c.euid != 0 && c.euid != child.uid && c.euid != dir.uid;
    }

    int VFS_Rename(RenameEnd& from, RenameEnd& to, const Credential& cred) {
        if (from.parent == nullptr || from.name == nullptr || to.parent == nullptr || to.name == nullptr)
            return -EINVAL;

        size_t oldNameLen = strlen(from.name);
        size_t newNameLen = strlen(to.name);
        if (oldNameLen == 0 || newNameLen == 0)
            return -EBUSY; // path was "/"
        if (oldNameLen > NAME_MAX || newNameLen > NAME_MAX)
            return -ENAMETOOLONG;

        auto isDot = [](const char* n, size_t len) {
            return (len == 1 && n[0] == '.') || (len == 2 && n[0] == '.' && n[1] == '.');
        };
        if (isDot(from.name, oldNameLen) || isDot(to.name, newNameLen))
            return -EBUSY;

        NSLockGuard ns;

        VNode* oldParent = nullptr;
        VNode* newParent = nullptr;
        VFS* vfs = nullptr;

        int rc = LookupPathLocked(from.parent, &oldParent, &vfs, from.cwd, cred, true);
        if (rc < 0)
            return rc;
        RefVNode(oldParent);
        VNodeRef oldParentRef(oldParent);

        rc = LookupPathLocked(to.parent, &newParent, &vfs, to.cwd, cred, true);
        if (rc < 0)
            return rc;
        RefVNode(newParent);
        VNodeRef newParentRef(newParent);

        if (oldParent->GetVFS() != newParent->GetVFS())
            return -EXDEV;

        rc = CheckCreate(oldParent, cred);
        if (rc < 0)
            return rc;

        if (newParent != oldParent) {
            rc = CheckCreate(newParent, cred);
            if (rc < 0)
                return rc;
        }

        // Lock parents in address order
        if (oldParent == newParent)
            oldParent->Lock();
        else if (oldParent < newParent) {
            oldParent->Lock();
            newParent->Lock();
        } else {
            newParent->Lock();
            oldParent->Lock();
        }

        auto unlockParents = [&]() {
            oldParent->Unlock();
            if (newParent != oldParent)
                newParent->Unlock();
        };

        if (oldParent->IsUnlinked() || newParent->IsUnlinked()) {
            unlockParents();
            return -ENOENT;
        }

        VNode* src = nullptr;
        rc = oldParent->Lookup(from.name, oldNameLen, &src, cred);
        if (rc < 0) {
            unlockParents();
            return rc;
        }
        RefVNode(src);
        VNodeRef srcRef(src);

        VNode* dst = nullptr;
        if (newParent->Lookup(to.name, newNameLen, &dst, cred) < 0)
            dst = nullptr;
        if (dst != nullptr)
            RefVNode(dst);
        VNodeRef dstRef(dst); // Unref of nullptr is a no-op, but the guard is fine

        if (src == dst) { // same object
            unlockParents();
            return ESUCCESS;
        }

        auto fail = [&](int err) {
            unlockParents();
            return err;
        };

        if (src->GetMountedVFS() != nullptr || (dst != nullptr && dst->GetMountedVFS() != nullptr))
            return fail(-EBUSY);

        VAttr oldPAttr, newPAttr, sattr;
        oldParent->GetAttr(&oldPAttr);
        newParent->GetAttr(&newPAttr);
        src->GetAttr(&sattr);
        bool srcIsDir = sattr.type == VType::DIR;

        if (!srcIsDir && (from.trailingSlash || to.trailingSlash))
            return fail(-ENOTDIR);

        if (StickyDenies(oldPAttr, sattr, cred))
            return fail(-EPERM);

        if (srcIsDir) {
            // newParent must not be src or inside src
            for (VNode* n = newParent; n != nullptr;) {
                if (n == src)
                    return fail(-EINVAL);
                VNode* up = n->GetParent();
                if (up == nullptr && n->GetVFS() != nullptr)
                    up = n->GetVFS()->GetCoveredVNode();
                n = up;
            }
        }

        if (dst != nullptr) {
            if (dst == oldParent)
                return fail(-ENOTEMPTY);

            dst->Lock();
            VAttr dattr;
            dst->GetAttr(&dattr);
            bool dstIsDir = dattr.type == VType::DIR;

            int err = ESUCCESS;
            if (srcIsDir && !dstIsDir)
                err = -ENOTDIR;
            else if (!srcIsDir && dstIsDir)
                err = -EISDIR;
            else if (dstIsDir && dst->HasChildren())
                err = -ENOTEMPTY;
            else if (StickyDenies(newPAttr, dattr, cred))
                err = -EPERM;
            if (err < 0) {
                dst->Unlock();
                return fail(err);
            }
        }

        src->Lock();
        rc = oldParent->Rename(src, newParent, to.name, newNameLen, dst, cred);
        if (rc >= 0 && dst != nullptr)
            dst->SetUnlinked(true);
        src->Unlock();
        if (dst != nullptr)
            dst->Unlock();
        unlockParents();
        return rc;
    }
}