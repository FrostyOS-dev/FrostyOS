/*
Copyright (©) 2025-2026  Frosty515

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

#ifndef _PROCESS_HPP
#define _PROCESS_HPP

#include <stdint.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/VMM.hpp>

#include "Thread.hpp"

enum class ProcessMode {
    KERNEL,
    USER
};

class Process {
public:
    Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice);
    ~Process();

    void CreateMainThread(ThreadEntryPoint entryPoint);
    void SetMainThread(Thread* thread);
    Thread* GetMainThread() const;

    uint64_t AddThread(Thread* thread);
    Thread* GetThread(uint64_t tid) const;
    void RemoveThread(uint64_t tid);

    void SwitchToThread(Thread* thread);

    ProcessMode GetMode() const;
    uint8_t GetNice() const;

    void SetPID(uint64_t pid);
    uint64_t GetPID() const;

    void SetPPID(uint64_t ppid);
    uint64_t GetPPID() const;

    void SetVMM(VMM::VMM* vmm);
    VMM::VMM* GetVMM() const;

private:
    ProcessMode m_Mode;
    VMM::VMM* m_VMM;
    uint8_t m_Nice;
    uint64_t m_PID;
    uint64_t m_PPID;
    uint64_t m_nextTID;
    Thread* m_MainThread;
    LinkedList::LockableLinkedList<Thread> m_Threads;
};


#endif /* _PROCESS_HPP */