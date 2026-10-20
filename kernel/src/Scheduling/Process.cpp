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
#include <string.h>
#include <spinlock.h>

#include <DataStructures/LinkedList.hpp>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <HAL/Processor.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>
#include <Memory/VMRegionAllocator.hpp>

#include <SystemCalls/File.hpp>
#include <SystemCalls/Signal.hpp>

#include <tty/TTYBackend.hpp>
#include <tty/TTY.hpp>


Process::Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice) : m_Mode(mode), m_VMM(vmm), m_Nice(nice), m_PID(-1), m_PPID(-1), m_nextTID(0), m_MainThread(nullptr), m_Threads(), m_cred({0, 0, 0, 0, 0, 0}), m_FDManager(nullptr), m_cwd(nullptr), m_state(ProcessState::ACTIVE), m_exitStatus(0), m_sigActions{}, m_pendingSignals{}, m_signalLock(SPINLOCK_DEFAULT_VALUE), m_stopSignal(0), m_waitEvents(0), m_continueNotify(0), m_exiting(false), m_PGID(-1), m_SID(-1), m_ctty(nullptr), m_hasExeced(false) {

}

Process::~Process() {
    if (m_cwd != nullptr)
        FS::UnrefVNode(m_cwd);
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
        if (in->Open(O_RDONLY) < 0 || out->Open(O_WRONLY) < 0 || err->Open(O_WRONLY) < 0 || debug->Open(O_WRONLY) < 0) {
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
    if (m_cwd != nullptr) {
        FS::UnrefVNode(m_cwd);
        m_cwd = nullptr;
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

FS::VNode* Process::GetCWDRef() {
    FS::VNode* vnode = m_cwd;
    FS::RefVNode(vnode);
    return vnode;
}

void Process::SetCWD(FS::VNode* cwd) {
    m_cwd = cwd;
}

bool Process::Fork(Process* other, uint64_t newMainReturn, CPU_Registers* regs) {
    memcpy(&m_sigActions, &other->m_sigActions, sizeof(sigaction_t) * NSIG);
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
    ProcessState s;
    __atomic_load(&m_state, &s, __ATOMIC_ACQUIRE);
    return s;
}

void Process::SetState(ProcessState state) {
    __atomic_store(&m_state, &state, __ATOMIC_RELEASE);
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

int64_t Process::GetPGID() const {
    return __atomic_load_n(&m_PGID, __ATOMIC_ACQUIRE);
}

void Process::SetPGID(int64_t pgid) {
    __atomic_store_n(&m_PGID, pgid, __ATOMIC_RELEASE);
}

int64_t Process::GetSID() const {
    return __atomic_load_n(&m_SID, __ATOMIC_ACQUIRE);
}

void Process::SetSID(int64_t sid) {
    __atomic_store_n(&m_SID, sid, __ATOMIC_RELEASE);
}

TTY* Process::GetControllingTTY() const {
    return m_ctty;
}

void Process::SetControllingTTY(TTY* tty) {
    m_ctty = tty;
}

bool Process::HasExeced() const {
    return m_hasExeced;
}

void Process::SetHasExeced(bool v) {
    m_hasExeced = v;
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

    if (GetState() == ProcessState::ZOMBIE)
        return 0;

    if (signal == SIGCONT)
        Resume();

    int state = Processor::DisableInterrupts();
    AcquireSignalLock();

    if (m_exiting) {
        ReleaseSignalLock();
        Processor::EnableInterrupts(state);
        return 0;
    }

    void* addr = m_sigActions[signal].address;
    if (signal == SIGCONT && (addr == SIG_DFL || addr == SIG_IGN)) {
        ReleaseSignalLock();
        Processor::EnableInterrupts(state);
        return 0;
    }

    if (signal == SIGSTOP || signal == SIGTSTP || signal == SIGTTIN || signal == SIGTTOU) {
        const int cont[] = { SIGCONT };
        DiscardPendingSignals(cont, 1); // stop cancels pending SIGCONT
    }

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

        if (SIGNAL_GET(&blocked, signal) == 0 || notIgnorable) {
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

    m_Threads.unlock();

    if (!notIgnorable && !(continued || shouldStop)) // no thread has it unmasked, so set it as pending for the whole process
        SIGNAL_SET(&m_pendingSignals, signal);

    spinlock_release(&m_signalLock);
    Processor::EnableInterrupts(state);

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

static const int s_stopSignals[] = { SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU };

static void ClearThreadPending(Thread* t, const int* sigs, size_t count) {
    t->AcquireSignalLock();
    for (size_t k = 0; k < count; k++) {
        int s = sigs[k];
        SIGNAL_CLEAR(&t->GetPendingSignals(), s);
    }
    t->ReleaseSignalLock();
}

void Process::Stop(int signal) {
    bool changed = false;
    int state = Processor::DisableInterrupts();
    AcquireSignalLock();
    if (GetState() == ProcessState::ACTIVE) {
        m_stopSignal = signal;
        __atomic_or_fetch(&m_waitEvents, PROCESS_WAIT_STOPPED, __ATOMIC_ACQ_REL);
        __atomic_and_fetch(&m_waitEvents, ~static_cast<uint32_t>(PROCESS_WAIT_CONTINUED), __ATOMIC_ACQ_REL);
        SetState(ProcessState::STOPPED);
        changed = true;
    }
    ReleaseSignalLock();
    Processor::EnableInterrupts(state);
    if (changed)
        NotifyParentStateChange();
}

bool Process::Resume() {
    bool wasStopped = false;
    int state = Processor::DisableInterrupts();
    AcquireSignalLock();
    DiscardPendingSignals(s_stopSignals, 4);
    if (GetState() == ProcessState::STOPPED) {
        SetState(ProcessState::ACTIVE);
        __atomic_and_fetch(&m_waitEvents, ~static_cast<uint32_t>(PROCESS_WAIT_STOPPED), __ATOMIC_ACQ_REL);
        __atomic_or_fetch(&m_waitEvents, PROCESS_WAIT_CONTINUED, __ATOMIC_ACQ_REL);
        __atomic_store_n(&m_continueNotify, 1, __ATOMIC_RELEASE);
        wasStopped = true;
    }
    ReleaseSignalLock();
    Processor::EnableInterrupts(state);
    if (wasStopped)
        m_continueQueue.Trigger(PROCESS_CONTINUE_EVENT);
    return wasStopped;
}

void Process::NotifyParentStateChange() {
    Process* parent = Scheduler::GetProcess(m_PPID);
    if (parent == nullptr)
        return;
    parent->GetChildWaitQueue().Trigger(PROCESS_EXIT_EVENT);
    if ((parent->GetSignalAction(SIGCHLD)->flags & SA_NOCLDSTOP) == 0)
        parent->RaiseSignal(SIGCHLD);
}

bool Process::TakeContinueNotify() {
    return __atomic_exchange_n(&m_continueNotify, 0, __ATOMIC_ACQ_REL) != 0;
}

int Process::GetStopSignal() const {
    return m_stopSignal;
}

uint32_t Process::GetWaitEvents() const {
    return __atomic_load_n(&m_waitEvents, __ATOMIC_ACQUIRE);
}

void Process::ClearWaitEvents(uint32_t events) {
    __atomic_and_fetch(&m_waitEvents, ~events, __ATOMIC_ACQ_REL);
}

EventWaitQueue& Process::GetContinueQueue() {
    return m_continueQueue;
}

void Process::MarkExiting() {
    int state = Processor::DisableInterrupts();
    AcquireSignalLock();
    m_exiting = true;
    ReleaseSignalLock();
    Processor::EnableInterrupts(state);
}

bool Process::IsExiting() const {
    return m_exiting;
}

void Process::DiscardPendingSignals(const int* sigs, size_t count) {
    for (size_t k = 0; k < count; k++) {
        int s = sigs[k];
        SIGNAL_CLEAR(&m_pendingSignals, s);
    }

    m_Threads.lock();
    if (m_MainThread != nullptr)
        ClearThreadPending(m_MainThread, sigs, count);
    m_Threads.Enumerate([&](Thread* t) -> ThreadList::IteratorDecision {
        ClearThreadPending(t, sigs, count);
        return ThreadList::IteratorDecision::Continue;
    });
    m_Threads.unlock();
}

Process* g_KProcess = nullptr;
