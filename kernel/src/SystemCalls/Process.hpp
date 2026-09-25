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

#include <HAL/HAL.hpp>

typedef long pid_t;
typedef int uid_t;
typedef int gid_t;

struct UIDs {
    uid_t ruid;
    uid_t euid;
    uid_t suid;
};

struct GIDs {
    gid_t rgid;
    gid_t egid;
    gid_t sgid;
};


[[noreturn]] void sys_exit(uint64_t code);

int sys_settcb(void* base);

pid_t sys_getpid();
pid_t sys_getppid();
pid_t sys_gettid();

int sys_getresuid(UIDs* uids);
int sys_getresgid(GIDs* gid);

pid_t sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, CPU_Registers* state);

int sys_exec(const char* path, char* const argv[], char* const env[]);

long sys_waitpid(pid_t pid, int* wstatus, int options);

#endif /* _SYSCALL_PROCESS_HPP */