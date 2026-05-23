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

#include "InitRAMFS.hpp"
#include "VFS.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <Scheduling/Process.hpp>

uint64_t StringOctalToNum(char* str, size_t len) {
    uint64_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == 0)
            break;
        n *= 8;
        n += str[i] - '0';
    }
    return n;
}

int LoadInitRAMFS(void* data, size_t size) {
    USTARItemHeader* header = (USTARItemHeader*)data;

    const Credential& cred = g_KProcess->GetCred();
    FS::VNode* cwd = nullptr;
    FS::VFS* cwdFS = nullptr;
    int rc = FS::VFS_LookupPath("/", &cwd, &cwdFS, nullptr, cred);
    if (rc < 0)
        return rc;

    dbgprintf("Loading initramfs...\n");

    uint64_t count = 0;

    while (memcmp(&header->ID, "ustar", 5) == 0 && ((uint64_t)header - (uint64_t)data) < size) {
        if (header->fileName[99] != 0)
            break;
        uint64_t itemSize = StringOctalToNum(header->size, 12);
        uint32_t uid = StringOctalToNum(header->uid, 8);
        uint32_t gid = StringOctalToNum(header->gid, 8);
        uint16_t mode = StringOctalToNum(header->mode, 8);
        uint32_t mtime = StringOctalToNum(header->mtime, 12);

        char* parent = (char*)"/";
        char* name = strrchr(header->fileName, '/');
        if (name != nullptr && name[1] == 0) {
            name[0] = 0;
            name = strrchr(header->fileName, '/');
        }
        if (name == nullptr)
            name = header->fileName;
        else {
            size_t length = (uint64_t)(name - header->fileName);
            parent = new char[length + 1];
            memcpy(parent, header->fileName, length);
            parent[length] = 0;
            name++; // after the /
        }


        switch (header->type) {
        case '0': { // File
            rc = FS::VFS_CreateFile(parent, name, cwd, cred);
            if (rc < 0)
                break;

            FS::VNode* vnode = nullptr;
            FS::VFS* vfs = nullptr;
            rc = FS::VFS_LookupPath(header->fileName, &vnode, &vfs, cwd, cred);
            if (rc < 0)
                break;

            FS::VAttr attr{};
            rc = vnode->GetAttr(&attr);
            if (rc < 0)
                break;

            attr.mode = mode;
            attr.uid = uid;
            attr.gid = gid;
            attr.atime = mtime;
            attr.mtime = mtime;
            attr.ctime = mtime;
            rc = vnode->SetAttr(attr);
            if (rc < 0)
                break;

            uint64_t bytes = 0;
            rc = vnode->Write((void*)((uint64_t)header + 512), itemSize, 0, 0, &bytes, cred);
            break;
        }
        case '5': { // Folder
            rc = FS::VFS_CreateDir(parent, name, cwd, cred);
            if (rc < 0)
                break;

            FS::VNode* vnode = nullptr;
            FS::VFS* vfs = nullptr;
            rc = FS::VFS_LookupPath(header->fileName, &vnode, &vfs, cwd, cred);
            if (rc < 0)
                break;

            FS::VAttr attr{};
            rc = vnode->GetAttr(&attr);
            if (rc < 0)
                break;

            attr.mode = mode;
            attr.uid = uid;
            attr.gid = gid;
            attr.atime = mtime;
            attr.mtime = mtime;
            attr.ctime = mtime;
            rc = vnode->SetAttr(attr);
            break;
        }
        default:
            rc = -ENOSYS;
            break;
        }

        if (name != header->fileName)
            delete[] parent;

        if (rc < 0)
            return rc;

        header = (USTARItemHeader*)((uint64_t)header + 512 + ALIGN_UP(itemSize, 512));
        count++;
    }

    dbgprintf("initramfs: loaded %lu items\n", count);

    return ESUCCESS;
}
