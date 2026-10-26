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

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

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

static size_t FieldLen(const char* field, size_t max) {
    size_t n = 0;
    while (n < max && field[n] != 0)
        n++;
    return n;
}

static int LoadEntry(USTARItemHeader* header, uint64_t itemSize, char* longName, const char* longLink, FS::VNode* cwd, const Credential& cred) {
    // Build the name once, on the stack
    char buf[sizeof(header->namePrefix) + 1 + sizeof(header->fileName) + 1];
    char* path;
    size_t len;

    if (longName != nullptr) {
        path = longName;
        len = strlen(longName);
    } else {
        path = buf;
        len = 0;
        if (header->ID[5] == 0) {
            len = FieldLen(header->namePrefix, sizeof(header->namePrefix));
            if (len > 0) {
                memcpy(buf, header->namePrefix, len);
                buf[len++] = '/';
            }
        }

        size_t nameLen = FieldLen(header->fileName, sizeof(header->fileName));
        memcpy(buf + len, header->fileName, nameLen);
        len += nameLen;
    }

    while (len > 1 && path[len - 1] == '/')
        len--;
    path[len] = 0;

    // Leading "./"
    while (path[0] && path[1] == '/') {
        path += 2;
        while (*path == '/')
            path++;
    }

    if (path[0] == 0 || (path[0] == '.' && path[1] == 0))
        return ESUCCESS;

    char* slash = strrchr(path, '/');
    const char* parent = "/";
    const char* name = path;
    if (slash != nullptr && slash != path) {
        *slash = 0;
        parent = path;
        name = slash + 1;
    } else if (slash == path)
        name = path + 1;

    int rc;
    FS::VNode* vnode = nullptr;
    switch (header->type) {
    case '0':
        rc = FS::VFS_Create(parent, name, FS::VType::REG, DEFAULT_FILE_MODE, cwd, cred, nullptr, &vnode);
        break;
    case '2': {
        char target[sizeof(header->linkedName) + 1];
        const char* linkTarget = longLink;
        if (linkTarget == nullptr) {
            size_t targetLen = FieldLen(header->linkedName, sizeof(header->linkedName));
            memcpy(target, header->linkedName, targetLen);
            target[targetLen] = 0;
            linkTarget = target;
        }
        rc = FS::VFS_Create(parent, name, FS::VType::LNK, DEFAULT_SYMLINK_MODE, cwd, cred, nullptr, &vnode, linkTarget);
        break;
    }
    case '5':
        rc = FS::VFS_Create(parent, name, FS::VType::DIR, DEFAULT_DIR_MODE, cwd, cred, nullptr, &vnode);
        break;
    default:
        dbgprintf("Unknown type %c. parent = %s, name = %s\n", header->type, parent, name);
        return -ENOSYS;
    }

    if (slash != nullptr && slash != path)
        *slash = '/';

    if (rc < 0)
        return rc;

    uint32_t mtime = StringOctalToNum(header->mtime, 12);

    FS::RefVNode(vnode);
    vnode->Lock();
    FS::VAttr attr{};
    rc = vnode->GetAttr(&attr);
    if (rc >= 0) {
        attr.mode = StringOctalToNum(header->mode, 8);
        attr.uid = StringOctalToNum(header->uid, 8);
        attr.gid = StringOctalToNum(header->gid, 8);
        attr.atime.tv_sec = mtime;
        attr.atime.tv_nsec = 0;
        attr.mtime = attr.atime;
        attr.ctime = attr.atime;
        rc = vnode->SetAttr(attr);
    }
    if (rc <= 0 && header->type == '0') {
        uint64_t bytes = 0;
        rc = vnode->Write((void*)((uint64_t)header + 512), itemSize, 0, 0, &bytes, cred);
    }
    vnode->Unlock();
    FS::UnrefVNode(vnode);
    return rc;
}

static int ReadLongField(USTARItemHeader* header, uint64_t itemSize, char*& out) {
    if (itemSize == 0 || itemSize > PATH_MAX)
        return -ENAMETOOLONG;
    delete[] out;
    out = new char[itemSize + 1];
    memcpy(out, (char*)header + 512, itemSize);
    out[itemSize] = 0;
    return ESUCCESS;
}

int LoadInitRAMFS(void* data, size_t size) {
    USTARItemHeader* header = (USTARItemHeader*)data;

    const uint64_t archiveStart = from_HHDM((uint64_t)data);
    const uint64_t archiveEnd = archiveStart + size;

    uint64_t freedUpTo = ALIGN_UP_BASE2(archiveStart, (uint64_t)PAGE_SIZE);

    auto releaseUpTo = [&](uint64_t limit) {
        limit = ALIGN_DOWN_BASE2(limit, (uint64_t)PAGE_SIZE);
        if (limit <= freedUpTo)
            return;
        g_PMM->ReclaimPages((void*)freedUpTo, (limit - freedUpTo) >> PAGE_SIZE_SHIFT);
        freedUpTo = limit;
    };

    const Credential& cred = g_KProcess->GetCred();
    FS::VNode* cwd = nullptr;
    FS::VFS* cwdFS = nullptr;
    int rc = FS::VFS_LookupPath("/", &cwd, &cwdFS, nullptr, cred);
    if (rc < 0)
        return rc;

    printf("Loading initramfs...\n");

    uint64_t count = 0;
    char* longName = nullptr;
    char* longLink = nullptr;
    rc = ESUCCESS;

    while (((uint64_t)header - (uint64_t)data) + sizeof(USTARItemHeader) <= size && memcmp(&header->ID, "ustar", 5) == 0) {
        uint64_t itemSize = StringOctalToNum(header->size, 12);

        uint64_t offset = (uint64_t)header - (uint64_t)data;
        if (itemSize > size - offset - 512) {
            rc = -EINVAL;
            break;
        }

        USTARItemHeader* next = (USTARItemHeader*)((uint64_t)header + 512 + ALIGN_UP(itemSize, 512));

        if (header->type == 'L' || header->type == 'K') {
            rc = ReadLongField(header, itemSize, header->type == 'L' ? longName : longLink);
            if (rc < 0)
                break;
            releaseUpTo(from_HHDM((uint64_t)next));
            header = next;
            continue;
        }

        rc = LoadEntry(header, itemSize, longName, longLink, cwd, cred);

        if (longName != nullptr) {
            delete[] longName;
            longName = nullptr;
        }
        if (longLink != nullptr) {
            delete[] longLink;
            longLink = nullptr;
        }

        if (rc < 0) {
            dbgprintf("Entry %lu loading failed: %s\n", count + 1, strerror(-rc));
            break;
        }

        releaseUpTo(from_HHDM((uint64_t)next)); // entire entry is consumed, must not be accessed again

        header = next;
        count++;
    }
    
    if (longName != nullptr)
        delete[] longName;
    if (longLink != nullptr)
        delete[] longLink;

    FS::UnrefVNode(cwd);
    releaseUpTo(ALIGN_UP_BASE2(archiveEnd, (uint64_t)PAGE_SIZE));

    if (rc < 0)
        return rc;

    printf("initramfs: loaded %lu items\n", count);
    return ESUCCESS;
}
