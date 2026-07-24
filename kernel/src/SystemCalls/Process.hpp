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

#ifndef _SYSCALL_PROCESS_HPP
#define _SYSCALL_PROCESS_HPP

#include <stdint.h>

typedef long pid_t;
typedef long uid_t;
typedef long gid_t;

[[noreturn]] void sys_exit(uint64_t code);

int sys_settcb(void* base);

pid_t sys_getpid();
pid_t sys_getppid();
pid_t sys_gettid();

uid_t sys_getuid();
uid_t sys_geteuid();

gid_t sys_getgid();
gid_t sys_getegid();

pid_t sys_fork();

int sys_exec(const char* path, char* const argv[], char* const env[]);

#endif /* _SYSCALL_PROCESS_HPP */