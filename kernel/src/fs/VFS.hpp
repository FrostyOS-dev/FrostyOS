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

#ifndef _VFS_HPP
#define _VFS_HPP

#include <stdint.h>
#include <time.h>

#include <Scheduling/Mutex.hpp>
#include <Scheduling/Process.hpp>

#define DEFAULT_DIR_MODE 0755
#define DEFAULT_FILE_MODE 0644

#define NAME_MAX 255
#define PATH_MAX 511

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

typedef int gid_t;
typedef int uid_t;
typedef unsigned int mode_t;
typedef int64_t ino_t;
typedef long off_t;
typedef uint64_t dev_t;
typedef unsigned long nlink_t;
typedef long blksize_t;
typedef uint64_t blkcnt_t;

/* Based on the Sun VFS Design */

namespace FS {

    class VNode;

    enum class FSType {
        TempFS,
        Invalid
    };

    enum class VType {
        NON,  // No type
        REG,  // Regular File
        DIR,  // Directory
        BLK,  // Block device
        CHR,  // Character device
        LNK,  // Symbolic link
        SOCK, // Socket
        FIFO, // Named pipe
        BAD   // Bad or dead file
    };

    struct VAttr {
        VType type;           // vnode type
        uint16_t mode;        // access mode
        uid_t uid;         // owner uid
        gid_t gid;         // owner gid
        FSType fsid;             // fs id
        ino_t inode;        // inode number
        int nlinks;           // number of links
        uint64_t size;        // file size
        uint64_t fsBlockSize; // block size
        timespec atime;       // last access time
        timespec mtime;       // last modification time
        timespec ctime;       // last change time
        uint64_t blocks;      // space used
    };

    struct Dentry {
        ino_t inode;
        off_t offset;
        uint16_t recordLen;
        uint8_t type;
        char name[NAME_MAX + 1];
    };

    class VFS {
    public:
        VFS();
        virtual ~VFS();

        virtual int Mount(int flags, void* backing, Credential cred) = 0;
        virtual int Unmount() = 0;
        virtual int StatFS() = 0;
        virtual int Sync() = 0;

        virtual VFS* GetNext();
        virtual VNode* GetCoveredVNode();
        virtual VNode* GetRoot();
        virtual FSType GetType() = 0;

    protected:
        VFS* m_next;
        VNode* m_nodeCovered;
        VNode* m_root;
        int m_flags; // TODO
    };

    class VNode {
    public:
        VNode(VFS* vfs);
        virtual ~VNode();

        virtual int Open(int flags, Credential cred) = 0;
        virtual int Close(int flags, Credential cred) = 0;
        virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, Credential cred) = 0;
        virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, Credential cred) = 0;
        virtual int Lookup(const char* name, size_t nameLen, VNode** out, Credential cred) = 0;
        virtual int Create(VNode* parent, const char* name, size_t nameLen, VAttr* attr, Credential cred) = 0;
        virtual int GetAttr(VAttr* out) = 0;
        virtual int SetAttr(const VAttr& attr) = 0;
        virtual int GetDents(Dentry* buffer, size_t count, uint64_t offset, size_t* readCount) = 0;
        virtual int Access() = 0;
        virtual int Link() = 0;
        virtual int Unlink() = 0;
        virtual int Symlink(const char* path, size_t pathLen, Credential cred) = 0;
        virtual int ReadLink(char* buffer, size_t size, Credential cred) = 0;
        virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, Credential cred) = 0;
        virtual int Munmap() = 0;
        virtual int Resize() = 0;
        virtual int Rename() = 0;
        virtual int GetName(char* buf, size_t size, size_t* realSize) = 0;

        virtual VFS* GetVFS();
        virtual VFS* GetMountedVFS();
        virtual VType GetType();
        virtual int& GetRefCount();
        virtual VNode* GetParent();

        virtual void Lock();
        virtual void Unlock();

    protected:
        VAttr m_attr;
        Mutex m_lock;
        int m_refCount;
        VFS* m_vfs; // Parent VFS
        VFS* m_vfsMounted; // VFS that is mounted here
        VNode* m_parent;
    };

    void RefVNode(VNode* node); // Increment refCount of a VNode
    void UnrefVNode(VNode* node); // Decrement refCount of a VNode, and delete it if refCount is 0.

    int VFS_Init();
    int VFS_MountRoot(FSType type, int flags, void* backing, Credential cred); // flags and backing are currently unusued
    int VFS_LookupPath(const char* path, VNode** vnode, VFS** vfs, VNode* cwd, Credential cred);

    int VFS_CreateDir(const char* path, const char* name, VNode* cwd, Credential cred);
    int VFS_CreateFile(const char* path, const char* name, VNode* cwd, Credential cred);
    int VFS_Open(const char* path, VNode** out, VNode* cwd, Credential cred);
    int VFS_Close(VNode* vnode, Credential cred);

    int VFS_CreateSymlink(const char* path, const char* name, const char* dest, VNode* cwd, Credential cred);

    // Map a vnode into memory. Flags are assumed to be pre-validated.
    int VFS_MapFile(void* hint, size_t length, VMM::Protection prot, int flags, bool user, VNode* vnode, uint64_t offset, void** addr, VMM::VMM* vmm, const Credential& cred);

    int VFS_BuildPath(VNode* vnode, char* buf, size_t size, Credential cred);

    uint8_t VFS_GetPosixType(VType type);

    extern VFS* g_rootVFS;

};

#endif /* _VFS_HPP */