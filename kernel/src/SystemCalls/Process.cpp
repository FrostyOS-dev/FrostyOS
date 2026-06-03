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

#include "Process.hpp"

#include <errno.h>
#include <stdint.h>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Thread.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Scheduling/TaskUtil.hpp>
#endif

[[noreturn]] void sys_exit(uint64_t code) {
    Thread::ExitCurrentThread(true, true);

    PANIC("sys_exit failed!");
}

int sys_settcb(void* base) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return -ENOSYS;

    if (!vmm->ValidateRead(base, 1))
        return -EFAULT;

#ifdef __x86_64__
    current->GetExtraContext()->fsBase = (uint64_t)base;
    x86_64_SetFSBase((uint64_t)base);
#endif

    return ESUCCESS;
}

pid_t sys_getpid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPID();
}

pid_t sys_getppid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPPID();
}

pid_t sys_gettid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPID(); // todo: unique TIDs between processes
}

uid_t sys_getuid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetCred().uid;
}

uid_t sys_geteuid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetCred().euid;
}

gid_t sys_getgid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetCred().gid;
}

gid_t sys_getegid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetCred().egid;
}
