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

#ifndef _x86_64_SYSCALL_HPP
#define _x86_64_SYSCALL_HPP

#include <stdint.h>

/*
On x86_64, system calls are issued with the following arguments:
rax: system call number
rdi: a
rsi: b
rdx: c
r8:  d
r9:  e
*/

extern "C" {
    void x86_64_SyscallEntry();
    
    uint64_t x86_64_SyscallHandler(uint64_t num, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e);
}

bool x86_64_InitSyscall();

#endif /* _x86_64_SYSCALL_HPP */