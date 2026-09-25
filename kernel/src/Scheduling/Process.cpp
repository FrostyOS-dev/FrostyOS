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

#include "Event.hpp"
#include "Process.hpp"
#include "Scheduler.hpp"
#include "Thread.hpp"
#include "ThreadList.hpp"

#include <errno.h>
#include <stdint.h>
#include <spinlock.h>

#include <DataStructures/LinkedList.hpp>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>
#include <Memory/VMRegionAllocator.hpp>

#include <SystemCalls/Signal.hpp>

#include <tty/TTYBackend.hpp>
#include <tty/TTY.hpp>


Process::Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice) : m_Mode(mode), m_VMM(vmm), m_Nice(nice), m_PID(-1), m_PPID(-1), m_nextTID(0), m_MainThread(nullptr), m_Threads(), m_cred({0, 0, 0, 0, 0, 0}), m_FDManager(nullptr), m_cwd(nullptr), m_state(ProcessState::ACTIVE), m_exitStatus(0) {

}

Process::~Process() {

}

bool Process::Start(bool insert) {
    if (insert)
        Scheduler::AddProcess(this);
    Scheduler::ScheduleThread(m_MainThread);
    m_Threads.lock();
    m_Threads.Enumerate([](Thread* thread, void*) -> ThreadList::IteratorDecision {
        Scheduler::ScheduleThread(thread);
        return ThreadList::IteratorDecision::Continue;
    }, nullptr);
    m_Threads.unlock();
    return true;
}

bool Process::Create(bool initAlloc) {
    if (m_VMM == nullptr) {
        VMRegionAllocator* alloc = new VMRegionAllocator();
        if (alloc == nullptr)
            return false;
        uint64_t start, end;
        GetDefaultUserRegion(&start, &end);
        if (initAlloc)
            alloc->Init(start, end);
        PageMapper* mapper = CreatePageMapper();
        if (mapper == nullptr) {
            delete alloc;
            return false;
        }
        VMM::VMM* vmm = new VMM::VMM(mapper, alloc);
        if (vmm == nullptr) {
            mapper->Delete();
            delete mapper;
            delete alloc;
            return false;
        }
        m_VMM = vmm;
    }
    if (m_FDManager == nullptr) {
        m_FDManager = new FileDescriptorManager();
        if (m_FDManager == nullptr) {
            Delete();
            return false;
        }
        if (!m_FDManager->Init()) {
            delete m_FDManager;
            m_FDManager = nullptr;
            Delete();
            return false;
        }
        FileDescriptor* in = new FileDescriptor(this, FDType::TTY, g_CurrentTTY, TTYStream::IN);
        FileDescriptor* out = new FileDescriptor(this, FDType::TTY, g_CurrentTTY, TTYStream::OUT);
        FileDescriptor* err = new FileDescriptor(this, FDType::TTY, g_CurrentTTY, TTYStream::OUT);
        FileDescriptor* debug = new FileDescriptor(this, FDType::TTY, g_CurrentTTY, TTYStream::DEBUG);
        if (in == nullptr || out == nullptr || err == nullptr || debug == nullptr) {
            delete in;
            delete out;
            delete err;
            delete debug;
            delete m_FDManager;
            m_FDManager = nullptr;
            Delete();
            return false;
        }
        if (!m_FDManager->ReserveFD(stdin, in) || !m_FDManager->ReserveFD(stdout, out) || !m_FDManager->ReserveFD(stderr, err) || !m_FDManager->ReserveFD(stddebug, debug)) {
            Delete();
            return false;
        }
        if (in->Open(0) < 0 || out->Open(0) < 0 || err->Open(0) < 0 || debug->Open(0) < 0) {
            Delete();
            return false;
        }
    }
    if (m_cwd == nullptr) {
        FS::VNode* vnode;
        FS::VFS* fs;
        int rc = FS::VFS_LookupPath("/", &vnode, &fs, nullptr, m_cred);
        if (rc == 0) // not having a cwd isn't fatal
            m_cwd = vnode;
    }
    return true;
}

void Process::Delete() {
    // TODO: delete all threads
    if (m_VMM != nullptr) {
        m_VMM->Delete(); // Clear the VMM mappings before deleting its mapper or allocator
        PageMapper* mapper = m_VMM->GetPageMapper();
        mapper->Delete();
        delete mapper;
        VMRegionAllocator* allocator = m_VMM->GetAllocator();
        allocator->Delete();
        delete allocator;
        delete m_VMM;
        m_VMM = nullptr;
    }
    if (m_FDManager != nullptr) {
        m_FDManager->Delete();
        delete m_FDManager;
        m_FDManager = nullptr;
    }
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
    m_Threads.lock();
    thread->SetTID(m_nextTID++);
    m_Threads.pushBack(thread);
    m_Threads.unlock();
    return thread->GetTID();
}

Thread* Process::GetThread(uint64_t tid) const {
    if (tid == 0)
        return m_MainThread;
    
    struct Data {
        Thread* thread;
        uint64_t tid;
    } data = {nullptr, tid};

    m_Threads.lock();
    m_Threads.EnumerateConst([](Thread* t, void* data) -> ThreadList::IteratorDecision {
        Data* d = (Data*)data;
        if (t->GetTID() == d->tid) {
            d->thread = t;
            return ThreadList::IteratorDecision::Break;
        }
        return ThreadList::IteratorDecision::Continue;
    }, nullptr);
    m_Threads.unlock();

    return data.thread;
}

void Process::RemoveThread(uint64_t tid, bool lock) {
    if (tid == 0)
        return; // Main thread

    if (lock)
        m_Threads.lock();
    m_Threads.Enumerate([](Thread* thread, void* data) -> ThreadList::IteratorDecision {
        uint64_t* tid = static_cast<uint64_t*>(data);
        if (thread->GetTID() == *tid)
            return ThreadList::IteratorDecision::Delete_Break;
        return ThreadList::IteratorDecision::Continue;
    }, &tid);
    if (lock)
        m_Threads.unlock();
}

void Process::RemoveThread(Thread* thread) {
    if (thread == nullptr || thread == m_MainThread)
        return;

    m_Threads.lock();
    m_Threads.remove(thread);
    m_Threads.unlock();
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

void Process::SetPID(int64_t pid) {
    m_PID = pid;
}

int64_t Process::GetPID() const {
    return m_PID;
}

void Process::SetPPID(int64_t ppid) {
    m_PPID = ppid;
}

int64_t Process::GetPPID() const {
    return m_PPID;
}

void Process::SetVMM(VMM::VMM* vmm) {
    m_VMM = vmm;
}

VMM::VMM* Process::GetVMM() const {
    return m_VMM;
}

const Credential& Process::GetCred() const {
    return m_cred;
}

void Process::SetCred(const Credential& cred) {
    m_cred = cred;
}

FileDescriptorManager* Process::GetFDManager() {
    return m_FDManager;
}

void Process::SetFDManager(FileDescriptorManager* manager) {
    m_FDManager = manager;
}

FS::VNode* Process::GetCWD() {
    return m_cwd;
}

void Process::SetCWD(FS::VNode* cwd) {
    m_cwd = cwd;
}

bool Process::Fork(Process* other, uint64_t newMainReturn, CPU_Registers* regs) {
    m_MainThread = new Thread();
    m_MainThread->SetParent(this);
    m_MainThread->SetTID(0);
    if (!m_MainThread->Fork(other->m_MainThread, newMainReturn, regs))
        return false;
    Scheduler::AddProcess(this);
    int state = Processor::DisableInterrupts();
    if (!Scheduler::AddExistingThread(m_MainThread)) {
        Processor::EnableInterrupts(state);
        Scheduler::RemoveProcess(m_PID);
        return false;
    }
    Processor::EnableInterrupts(state);
    return true;
}

AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& Process::GetFutextList() {
    return m_futexList;
}

ProcessState Process::GetState() const {
    return m_state;
}

void Process::SetState(ProcessState state) {
    m_state = state;
}

int Process::GetExitStatus() const {
    return m_exitStatus;
}

void Process::SetExitStatus(int status) {
    m_exitStatus = status;
}

EventWaitQueue& Process::GetChildWaitQueue() {
    return m_childWaitQueue;
}

int Process::SetSignalAction(int signal, sigaction_t* newAct, sigaction_t* oldAct) {
    if (signal <= 0 || signal >= NSIG)
        return -EINVAL;

    int state = Processor::DisableInterrupts();
    AcquireSignalLock();

    if (oldAct != nullptr)
        *oldAct = m_sigActions[signal];

    if (newAct != nullptr)
        m_sigActions[signal] = *newAct;

    ReleaseSignalLock();
    Processor::EnableInterrupts(state);

    return ESUCCESS;
}

int Process::RaiseSignal(int signal) {
    if (signal <= 0 || signal >= NSIG)
        return -EINVAL;

    int state = Processor::DisableInterrupts();
    AcquireSignalLock();

    void* addr = m_sigActions[signal].address;
    bool notIgnored = addr != SIG_IGN && ((addr == SIG_DFL && g_signalDefaultActions[signal] != SIGACTION_IGN) || addr != SIG_DFL);
    bool notIgnorable = signal == SIGKILL || signal == SIGSTOP || signal == SIGCONT;
    bool continued = false;
    bool isDefaultAct = addr == SIG_DFL;
    bool shouldStop = (isDefaultAct && g_signalDefaultActions[signal] == SIGACTION_STOP) || signal == SIGSTOP;

    m_Threads.lock();
    Thread* thread = m_MainThread;

    while (thread != nullptr) {
        if (thread->PendingDelete()) {
            if (thread == m_MainThread)
                thread = m_Threads.getHead();
            else
                thread = m_Threads.getNext(thread);
            continue;
        }

        thread->AcquireSignalLock();

        sigset_t& blocked = thread->GetBlockedSignals();
        sigset_t& pending = thread->GetPendingSignals();

        if (SIGNAL_GET(&blocked, signal) == 0 || notIgnorable) {
            // SIGNAL_SET(&pending, signal);
            // if (notIgnored || notIgnorable) {
            //     // wake-up, including stopped check for SIGCONT
            // }

            // if (!(shouldStop || signal == SIGCONT)) { // stopping or continuing, possible race prevention
            //     m_Threads.unlock();
            //     thread->ReleaseSignalLock();
            //     spinlock_release(&m_signalLock);
            //     Processor::EnableInterrupts(state);
            //     return 0;
            // }
            thread->ReleaseSignalLock();
            m_Threads.unlock();
            spinlock_release(&m_signalLock);

            thread->RaiseSignal(signal);
            Processor::EnableInterrupts(state);
            return 0;
        }

        thread->ReleaseSignalLock();

        if (thread == m_MainThread)
            thread = m_Threads.getHead();
        else
            thread = m_Threads.getNext(thread);
    }

    // TODO: no threads have it unmasked, check for threads waiting for the signal

    m_Threads.unlock();

    if (!notIgnorable && !(continued || shouldStop)) // no thread has it unmasked, so set it as pending for the whole process
        SIGNAL_SET(&m_pendingSignals, signal);

    spinlock_release(&m_signalLock);
    Processor::EnableInterrupts(state);

    // tell the parent when a child stopped
    // if ((notIgnorable || notIgnored) && g_signalDefaultActions[signal] == SIGACTION_STOP) {
    //     // TODO: proc status

    //     Process* parent = Scheduler::GetProcess(m_PPID);
    //     if (parent != nullptr && (parent->m_sigActions[SIGCHLD].flags & SA_NOCLDSTOP) == 0)
    //         parent->RaiseSignal(SIGCHLD);
    // }

    return 0;
}

sigset_t& Process::GetPendingSignals() {
    return m_pendingSignals;
}

sigaction_t* Process::GetSignalAction(int signal) {
    return &m_sigActions[signal];
}

void Process::AcquireSignalLock() {
    spinlock_acquire(&m_signalLock);
}

void Process::ReleaseSignalLock() {
    spinlock_release(&m_signalLock);
}


Process* g_KProcess = nullptr;
