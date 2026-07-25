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

#ifndef _SYSCALL_FUTEX_HPP
#define _SYSCALL_FUTEX_HPP

#include <spinlock.h>
#include <stdint.h>
#include <time.h>

#include <Scheduling/Thread.hpp>
#include <Scheduling/ThreadList.hpp>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

class Process;

class FutexWaitQueue {
public:
    FutexWaitQueue();
    ~FutexWaitQueue();

    int Wait(uint32_t* futexPtr, Process* proc, uint32_t expected, timespec* tm);
    int Wake(uint32_t maxCount);
    void Remove(Thread* thread, FutexWakeReason reason);

    uint64_t refCount;

private:
    ThreadList m_waitingThreads;
    spinlock_t m_lock;
};

int sys_futex(int operation, uint32_t* futexPtr, uint32_t value, timespec* tm);


#endif /* _SYSCALL_FUTEX_HPP */