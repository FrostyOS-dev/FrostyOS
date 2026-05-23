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

#ifndef _INITRAMFS_HPP
#define _INITRAMFS_HPP

#include <stddef.h>

struct [[gnu::packed]] USTARItemHeader {
    char fileName[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkedName[100];
    char ID[6];
    char version[2];
    char ownerName[32];
    char ownerGroup[32];
    char devMajor[8];
    char devMinor[8];
    char namePrefix[155];
};

int LoadInitRAMFS(void* data, size_t size);

#endif /* _INITRAMFS_HPP */