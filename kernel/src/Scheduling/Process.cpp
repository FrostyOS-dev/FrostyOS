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

#include "Process.hpp"
#include "Scheduler.hpp"
#include "Thread.hpp"

#include <stdint.h>
#include <spinlock.h>

#include <DataStructures/LinkedList.hpp>

Process::Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice) : m_Mode(mode), m_VMM(vmm), m_Nice(nice), m_PID(UINT64_MAX), m_PPID(UINT64_MAX), m_nextTID(0), m_MainThread(nullptr), m_Threads(), m_threadsLock(SPINLOCK_DEFAULT_VALUE), m_intState(-1) {

}

Process::~Process() {

}

bool Process::Start() {
    Scheduler::AddProcess(this);
    Scheduler::ScheduleThread(m_MainThread);
    LockThreadList();
    m_Threads.Enumerate([](Thread* thread, void*) -> bool {
        Scheduler::ScheduleThread(thread);
        return true;
    }, nullptr);
    UnlockThreadList();
    return true;
}

bool Process::CreateMainThread(ThreadEntryPoint entryPoint) {
    m_MainThread = new Thread(entryPoint, this, 0);
    return m_MainThread->Init();
}

void Process::SetMainThread(Thread* thread) {
    m_MainThread = thread;
    thread->SetTID(0);
}

Thread* Process::GetMainThread() const {
    return m_MainThread;
}

uint64_t Process::AddThread(Thread* thread) {
    LockThreadList();
    thread->SetTID(m_nextTID++);
    m_Threads.insert(thread);
    UnlockThreadList();
    return thread->GetTID();
}

Thread* Process::GetThread(uint64_t tid) const {
    if (tid == 0)
        return m_MainThread;
    
    struct Data {
        Thread* thread;
        uint64_t tid;
    } data = {nullptr, tid};

    LockThreadList();
    m_Threads.Enumerate([](Thread* t, void* data) -> bool {
        Data* d = (Data*)data;
        if (t->GetTID() == d->tid) {
            d->thread = t;
            return false;
        }
        return true;
    }, nullptr);
    UnlockThreadList();

    return data.thread;
}

void Process::RemoveThread(uint64_t tid, bool lock) {
    if (tid == 0)
        return; // Main thread

    if (lock)
        LockThreadList();
    m_Threads.EnumerateDelete([](Thread* thread, void* data, uint64_t) -> LinkedList::IteratorDecision {
        uint64_t* tid = static_cast<uint64_t*>(data);
        if (thread->GetTID() == *tid)
            return LinkedList::IteratorDecision::DELETE_BREAK;
        return LinkedList::IteratorDecision::CONTINUE;
    }, &tid);
    if (lock)
        UnlockThreadList();
}

void Process::LockThreadList() const {
    int intState = Processor::DisableInterrupts();
    spinlock_acquire(&m_threadsLock);
    m_intState = intState;
}

void Process::UnlockThreadList() const {
    int intState = m_intState;
    spinlock_release(&m_threadsLock);
    Processor::EnableInterrupts(intState);
}

void Process::SwitchToThread(Thread* thread) {
    // TODO
}

ProcessMode Process::GetMode() const {
    return m_Mode;
}

uint8_t Process::GetNice() const {
    return m_Nice;
}

void Process::SetPID(uint64_t pid) {
    m_PID = pid;
}

uint64_t Process::GetPID() const {
    return m_PID;
}

void Process::SetPPID(uint64_t ppid) {
    m_PPID = ppid;
}

uint64_t Process::GetPPID() const {
    return m_PPID;
}

void Process::SetVMM(VMM::VMM* vmm) {
    m_VMM = vmm;
}

VMM::VMM* Process::GetVMM() const {
    return m_VMM;
}

Process* g_KProcess = nullptr;
Process* g_KLowestPriorityProcess = nullptr;
