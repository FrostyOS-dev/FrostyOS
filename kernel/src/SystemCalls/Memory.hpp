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

#ifndef _SYSCALL_MEMORY_HPP
#define _SYSCALL_MEMORY_HPP

#include <stddef.h>
#include <stdint.h>

typedef int64_t off_t;

struct [[gnu::packed]] sys_mmapExtraArgs {
    int fd;
    off_t offset;
};

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_PRIVATE 1
#define MAP_ANONYMOUS 2
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FIXED 4

void* sys_mmap(void* addr, size_t length, int prot, int flags, sys_mmapExtraArgs* args);
int sys_munmap(void* addr, size_t length); // For now, the bounds of this must be the bounds of the original mapping
int sys_mprotect(void* addr, size_t size, int prot); // For now, the bounds of this must be the bounds of the original mapping

#endif /* _SYSCALL_MEMORY_HPP */