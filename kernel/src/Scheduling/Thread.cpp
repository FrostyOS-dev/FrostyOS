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
#include "SystemCalls/Signal.hpp"
#include "ThreadList.hpp"
#include "arch/x86_64/Scheduling/TaskUtil.hpp"

#include <errno.h>
#include <spinlock.h>
#include <string.h>
#include <util.h>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>

#include <SystemCalls/Futex.hpp>

Thread::Thread() : m_EntryPoint({nullptr, nullptr}), m_Parent(nullptr), m_TID(UINT64_MAX), m_Stack(0), m_KernelStack(0), m_ThreadListData{nullptr, nullptr}, m_ProcThreadListData{nullptr, nullptr}, m_TimeRemaining(0), m_CPUInfo(nullptr, SPINLOCK_DEFAULT_VALUE), m_InSchedList(false), m_InProcList(false), m_IsSleeping(false), m_deleteProp(false, false, true, false, -1) {
    sleepRemainingTime = 0;
    yieldCallback = {nullptr, nullptr};
    m_InSchedList = false;
    m_InProcList = false;

}

Thread::Thread(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) : m_EntryPoint(entryPoint), m_Parent(parent), m_TID(tid), m_Stack(0), m_KernelStack(0), m_ThreadListData{nullptr, nullptr}, m_ProcThreadListData{nullptr, nullptr}, m_TimeRemaining(0), m_CPUInfo(nullptr, SPINLOCK_DEFAULT_VALUE), m_InSchedList(false), m_InProcList(false), m_IsSleeping(false), m_deleteProp(false, false, true, false, -1) {
    sleepRemainingTime = 0;
    yieldCallback = {nullptr, nullptr};

    m_InSchedList = false;
    m_InProcList = false;
}

Thread::~Thread() {

}

bool Thread::Init() {
    if (m_Parent == nullptr)
        return false;

    return CreateStacks();
}

bool Thread::Init(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) {
    m_EntryPoint = entryPoint;
    m_Parent = parent;
    m_TID = tid;
    m_Stack = 0;
    return Init();
}

bool Thread::Delete() {
    if (m_Parent == nullptr)
        return false;

    VMM::VMM* vmm = m_Parent->GetVMM();
    if (vmm == nullptr)
        return false;

    if (!vmm->FreePages(reinterpret_cast<void*>(m_KernelStack - KERNEL_STACK_SIZE)))
        return false;

    if (m_Parent->GetMode() == ProcessMode::USER && !vmm->FreePages(reinterpret_cast<void*>(m_Stack - DEFAULT_USER_STACK_SIZE)))
        return false;

    if (blockedFutex != nullptr)
        blockedFutex->Remove(this, FutexWakeReason::Interrupted);

    m_Stack = 0;
    m_KernelStack = 0;
    return true;
}

bool Thread::ExitCurrentThread(bool deleteThis, bool deleteParent, bool removeProc) {
    int exitIntState = Processor::DisableInterrupts();
    Thread* thread = Scheduler::RemoveCurrentThread(true);
    if (thread == nullptr)
        return false;
    // Clear any pending yield callback to prevent use-after-free when thread is deleted
    thread->yieldCallback = {};
    thread->m_deleteProp = {deleteThis, deleteParent, removeProc, true, exitIntState};
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    Processor::SwapStack(Thread_ExitHelper, thread, state->kernelStack);
    return false;
}

Thread* Thread::GetCurrentThread() {
    int intState = Processor::DisableInterrupts();
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    Thread* thread = state->currentThread;
    Processor::EnableInterrupts(intState);
    return thread;
}

void Thread::SetEntryPoint(ThreadEntryPoint entryPoint) {
    m_EntryPoint = entryPoint;
}

ThreadEntryPoint Thread::GetEntryPoint() const {
    return m_EntryPoint;
}

void Thread::SetParent(Process* parent) {
    m_Parent = parent;
}

Process* Thread::GetParent() const {
    return m_Parent;
}

void Thread::SetTID(uint64_t tid) {
    m_TID = tid;
}

uint64_t Thread::GetTID() const {
    return m_TID;
}

void Thread::SetRegisters(CPU_Registers& registers) {
    memcpy(&m_Registers, &registers, sizeof(CPU_Registers));
}

const CPU_Registers& Thread::GetRegisters() const {
    return m_Registers;
}

CPU_Registers& Thread::GetMutableRegisters() {
    return m_Registers;
}

CPU_ExtraContext* Thread::GetExtraContext() {
    return &m_extraContext;
}

bool Thread::CreateStacks() {
    if (m_Parent == nullptr)
        return false;

    VMM::VMM* vmm = m_Parent->GetVMM();
    if (vmm == nullptr)
        return false;

    void* stack = VMM::g_KVMM->AllocateAnonPages(DIV_ROUNDUP(KERNEL_STACK_SIZE, PAGE_SIZE), VMM::DEFAULT_KALLOC_PHYS_FLAGS);
    if (stack == nullptr)
        return false;
    m_KernelStack = reinterpret_cast<uint64_t>(stack) + KERNEL_STACK_SIZE;

    if (m_Parent->GetMode() == ProcessMode::USER) {
        stack = vmm->AllocateAnonPages(DIV_ROUNDUP(DEFAULT_USER_STACK_SIZE, PAGE_SIZE), VMM::DEFAULT_ALLOC_FLAGS);
        if (stack == nullptr)
            return false;
        m_Stack = reinterpret_cast<uint64_t>(stack) + DEFAULT_USER_STACK_SIZE;
    } else
        m_Stack = m_KernelStack;

    return true;
}

void Thread::SetStack(uint64_t stack) {
    m_Stack = stack;
}

uint64_t Thread::GetStack() const {
    return m_Stack;
}

void Thread::SetKernelStack(uint64_t stack) {
    m_KernelStack = stack;
}

uint64_t Thread::GetKernelStack() const {
    return m_KernelStack;
}

void Thread::SetThreadListData(ThreadListItemInternalData& data) {
    m_ThreadListData = data;
}

void Thread::SetInSchedList(bool inList) {
    m_InSchedList = inList;
}

bool Thread::IsInSchedList() const {
    return m_InSchedList;
}

void Thread::SetInProcList(bool inList) {
    m_InProcList = inList;
}

bool Thread::IsInProcList() const {
    return m_InProcList;
}

ThreadListItemInternalData& Thread::GetThreadListData() {
    return m_ThreadListData;
}

void Thread::SetProcThreadListData(ThreadListItemInternalData& data) {
    m_ProcThreadListData = data;
}

ThreadListItemInternalData& Thread::GetProcThreadListData() {
    return m_ProcThreadListData;
}

void Thread::SetTimeRemaining(uint64_t timeRemaining) {
    m_TimeRemaining = timeRemaining;
}

uint64_t Thread::GetTimeRemaining() const {
    return m_TimeRemaining;
}

Thread::CPUInfo* Thread::GetCPUInfo() {
    return &m_CPUInfo;
}

void Thread::SetDeleteProp(bool deleteSelf, bool deleteParent) {
    m_deleteProp.deleteThis = deleteSelf;
    m_deleteProp.deleteParent = deleteParent;
}

bool Thread::ShouldDelete() const {
    return m_deleteProp.deleteThis;
}

bool Thread::ShouldDeleteParent() const {
    return m_deleteProp.deleteParent;
}

bool Thread::ShouldRemoveProc() const {
    return m_deleteProp.removeProc;
}

bool Thread::PendingDelete() const {
    return m_deleteProp.pendingDelete;
}

int64_t Thread::GetIntState() const {
    return m_deleteProp.intState;
}

bool Thread::Fork(Thread* other, uint64_t newReturnValue) {
    // start with the stacks, then copy everything else
    if (m_Parent == nullptr)
        return false;

    VMM::VMM* vmm = m_Parent->GetVMM();
    if (vmm == nullptr)
        return false;

    // Need to create a new kernel stack as it is kernel address space
    void* stack = VMM::g_KVMM->AllocateAnonPages(DIV_ROUNDUP(KERNEL_STACK_SIZE, PAGE_SIZE), VMM::DEFAULT_KALLOC_PHYS_FLAGS);
    if (stack == nullptr)
        return false;
    m_KernelStack = reinterpret_cast<uint64_t>(stack) + KERNEL_STACK_SIZE;

    // Don't need to create a new user stack as the entire user address space is duplicated
    m_Stack = other->m_Stack;

    m_EntryPoint = other->m_EntryPoint;
    m_TimeRemaining = 0;
    
    memcpy(&m_deleteProp, &other->m_deleteProp, sizeof(m_deleteProp));

    int state = Processor::DisableInterrupts();
    Processor* proc = GetCurrentProcessor();
    proc->InitExtraContext(&m_extraContext);
    proc->CopyExtraContext(&m_extraContext, &other->m_extraContext);
    Processor::EnableInterrupts(state);
    
    PageMapper* pageMapper = vmm->GetPageMapper();
    Processor::ForkRegisters(&m_Registers, &other->m_Registers, 0, pageMapper->GetPageTable());

    return true;
}

int Thread::RaiseSignal(int signal) {
    if (signal == 0 || signal >= NSIG)
        return -EINVAL;

    assert(m_Parent != nullptr);
    int state = Processor::DisableInterrupts();
    m_Parent->AcquireSignalLock();
    spinlock_acquire(&m_signalLock);

    sigset_t* set = &m_pendingSignals;
    SIGNAL_SET(set, signal);

    sigaction_t* act = m_Parent->GetSignalAction(signal);
    void* addr = act->address;
    bool notIgnored = addr != SIG_IGN && ((addr == SIG_DFL && g_signalDefaultActions[signal] != SIGACTION_IGN) || addr != SIG_DFL);

    if (signal == SIGKILL || signal == SIGSTOP || (SIGNAL_GET(&m_blockedSignals, signal) == 0 && notIgnored)) {
        // signal must be issued immediately
        if (m_IsSleeping) {
            m_IsSleeping = false;
            sleepRemainingTime = 0;
            Scheduler::AddExistingThread(this);
        } else if (blockedFutex != nullptr)
            blockedFutex->Remove(this, FutexWakeReason::Interrupted);
    } else if (!notIgnored) {
        // ignored, doesn't need to be pending anymore
        SIGNAL_CLEAR(set, signal);
    }

    spinlock_release(&m_signalLock);
    m_Parent->ReleaseSignalLock();
    Processor::EnableInterrupts(state);
    return 0;
}

int Thread::CheckSignals() {
    if (m_inSignalHandler)
        return 0; // Prevent signal nesting

    int state = Processor::DisableInterrupts();

    int signum = 0;
    sigset_t pending;
    GetPendingSignals(&pending); // Uses locks internally

    for (int i = 1; i < NSIG; i++) {
        if (SIGNAL_GET(&m_blockedSignals, i) == 0 && SIGNAL_GET(&pending, i) > 0) {
            signum = i;

            // Clear from thread or process pending mask
            AcquireSignalLock();
            if (SIGNAL_GET(&m_pendingSignals, i) > 0)
                SIGNAL_CLEAR(&m_pendingSignals, i);
            else {
                m_Parent->AcquireSignalLock();
                SIGNAL_CLEAR(&m_Parent->GetPendingSignals(), i);
                m_Parent->ReleaseSignalLock();
            }
            ReleaseSignalLock();
            break;
        }
    }

    Processor::EnableInterrupts(state);

    if (signum == 0)
        return 0;

    sigaction_t act = *m_Parent->GetSignalAction(signum);
    void* handler = act.address;

    if (handler == SIG_IGN)
        return 0;

    if (handler == SIG_DFL) {
        int defAct = g_signalDefaultActions[signum];
        if (defAct == SIGACTION_TERM || defAct == SIGACTION_CORE)
            ExitCurrentThread(true, m_Parent->GetMainThread() == this, true);
        return 0;
    }

    return signum; // returns postitive integer to indicate a signal frame needs to be built
}

int Thread::DispatchSignals(CPU_Registers* regs, CPU_ExtraContext* extra) {
    if (regs == nullptr || extra == nullptr || m_Parent == nullptr)
        return -EINVAL;

    // CheckSignals returns the signal number if a user handler needs to be invoked
    int signum = CheckSignals();
    if (signum <= 0)
        return 0; // no userspace frame needs to be built

    sigaction_t* act = m_Parent->GetSignalAction(signum);
    if (act == nullptr)
        return -EINVAL;

    sigset_t oldBlocked = m_blockedSignals;
    sigset_t newBlocked = m_blockedSignals;

    // Apply action's specific mask to the thread while running the handler
    for (int i = 1; i < NSIG; i++) {
        if (SIGNAL_GET(&act->mask, i) > 0)
            SIGNAL_SET(&newBlocked, i);
    }

    // Block the current signal during execution unless SA_NODEFER is specified
    if ((act->flags & SA_NODEFER) == 0)
        SIGNAL_SET(&newBlocked, signum);

    int rc = -ENOSYS;

#ifdef __x86_64__
    rc = x86_64_SetupSignalFrame(m_Parent, regs, extra, act, &oldBlocked, signum);
#endif /* __x86_64__ */

    if (rc == 0) {
        // Safely apply new mask
        ChangeSignalMask(SIG_SETMASK, &newBlocked, nullptr);

        m_inSignalHandler = true; // no nesting of signals

        // Reset signal action to default if requested
        if ((act->flags & SA_RESETHAND) > 0) {
            sigaction_t defAct;
            memset(&defAct, 0, sizeof(sigaction_t));
            defAct.address = SIG_DFL;

            m_Parent->SetSignalAction(signum, &defAct, nullptr);
        }
    } else {
        // Frame setup failed, force SIGSEGV
        sigaction_t defAct;
        memset(&defAct, 0, sizeof(sigaction_t));
        defAct.address = SIG_DFL;
        m_Parent->SetSignalAction(SIGSEGV, &defAct, nullptr);
        RaiseSignal(SIGSEGV);
    }

    return signum;
}

int Thread::RestoreSignalContext(CPU_Registers* regs, CPU_ExtraContext* extra) {
    if (regs == nullptr || extra == nullptr)
        return -EINVAL;

    if (!m_inSignalHandler) // ensure we are actually returning from a handler
        return -EPERM;

    sigset_t restoredMask;
    int rc = -ENOSYS;

#ifdef __x86_64__
    rc = x86_64_RestoreSignalFrame(m_Parent, regs, extra, &restoredMask, regs->RSP);
#endif /* __x86_64__ */

    if (rc == 0) {
        ChangeSignalMask(SIG_SETMASK, &restoredMask, nullptr); // restore mask

        m_inSignalHandler = false; // allow new signals
    } else { // Failed to restore, force terminate the process with SIGSEGV
        sigaction_t defAct;
        memset(&defAct, 0, sizeof(sigaction_t));
        defAct.address = SIG_DFL;
        m_Parent->SetSignalAction(SIGSEGV, &defAct, nullptr);
        RaiseSignal(SIGSEGV);
    }

    dbgprintf("Returning from signal\n");

    return rc;
}

void Thread::ChangeSignalMask(int how, sigset_t* newSet, sigset_t* oldSet) {
    int state = Processor::DisableInterrupts();
    spinlock_acquire(&m_signalLock);

    if (oldSet != nullptr)
        *oldSet = m_blockedSignals;

    if (newSet != nullptr) {
        switch (how) {
        case SIG_BLOCK:
        case SIG_UNBLOCK: {
            for (int i = 1; i < NSIG; i++) {
                if (SIGNAL_GET(newSet, i) == 0)
                    continue;

                if (how == SIG_BLOCK && SIGNAL_GET(&m_blockedSignals, i) == 0)
                    SIGNAL_SET(&m_blockedSignals, i);
                else if (how == SIG_UNBLOCK && SIGNAL_GET(&m_blockedSignals, i) > 0)
                    SIGNAL_CLEAR(&m_blockedSignals, i);
            }
            break;
        }
        case SIG_SETMASK: {
            m_blockedSignals = *newSet;
            break;
        }
        }
    }

    spinlock_release(&m_signalLock);
    Processor::EnableInterrupts(state);
}

void Thread::GetPendingSignals(sigset_t* set) {
    assert(m_Parent != nullptr);
    int state = Processor::DisableInterrupts();
    m_Parent->AcquireSignalLock();
    spinlock_acquire(&m_signalLock);

    sigset_t& parentPending = m_Parent->GetPendingSignals();

    memset(set, 0, sizeof(sigset_t));

    for (int i = 1; i < NSIG; i++) {
        if (SIGNAL_GET(&parentPending, i) > 0)
            SIGNAL_SET(set, i);

        if (SIGNAL_GET(&m_pendingSignals, i) > 0)
            SIGNAL_SET(set, i);
    }

    spinlock_release(&m_signalLock);
    m_Parent->ReleaseSignalLock();
    Processor::EnableInterrupts(state);
}

sigset_t& Thread::GetBlockedSignals() {
    return m_blockedSignals;
}

sigset_t& Thread::GetPendingSignals() {
    return m_pendingSignals;
}

void Thread::AcquireSignalLock() {
    spinlock_acquire(&m_signalLock);
}

void Thread::ReleaseSignalLock() {
    spinlock_release(&m_signalLock);
}

[[noreturn]] void Thread_ExitHelper(void* data) {
    Thread* thread = static_cast<Thread*>(data);
    if (!Scheduler::DeleteThread(thread))
        PANIC("Failed to delete thread on exit!");

    Scheduler::Yield(nullptr, false, nullptr, false);
    PANIC("Scheduler::Yield() returned!");
}
