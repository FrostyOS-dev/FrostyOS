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

#ifndef _SYSCALL_FILE_HPP
#define _SYSCALL_FILE_HPP

#include <stddef.h>

typedef unsigned int mode_t;
typedef long ssize_t;
typedef long off_t;

#define O_PATH 010000000

#define O_ACCMODE (03 | O_PATH)
#define O_RDONLY   00
#define O_WRONLY   01
#define O_RDWR     02

#define O_CREAT         0100
#define O_EXCL          0200
#define O_NOCTTY        0400
#define O_TRUNC        01000
#define O_APPEND       02000
#define O_NONBLOCK     04000
#define O_DSYNC       010000
#define O_ASYNC       020000
#define O_DIRECT      040000
#define O_LARGEFILE  0100000
#define O_DIRECTORY  0200000
#define O_NOFOLLOW   0400000
#define O_NOATIME   01000000
#define O_CLOEXEC   02000000
#define O_SYNC      04010000
#define O_RSYNC     04010000

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// pathLen does NOT include null-termination
int sys_open(const char* path, size_t pathLen, int flags, mode_t mode);
int sys_close(int fd);

ssize_t sys_read(int fd, void* buf, size_t count);
ssize_t sys_write(int fd, const void* buf, size_t count);

off_t sys_seek(int fd, off_t offset, int whence);

int sys_isatty(int fd);

int sys_getdents(int fd, void* buf, size_t maxRead, size_t* bytesRead);

int sys_getcwd(char* buf, size_t size);

#endif /* _SYSCALL_FILE_HPP */