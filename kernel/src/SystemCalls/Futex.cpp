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

#include "Futex.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stdint.h>
#include <time.h>

#include <DataStructures/AVLTree.hpp>

#include <HAL/Processor.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>

#include <Scheduling/Mutex.hpp>
#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>
#include <Scheduling/ThreadList.hpp>


FutexWaitQueue::FutexWaitQueue() : refCount(1), m_waitingThreads(), m_lock(SPINLOCK_DEFAULT_VALUE) {
}

FutexWaitQueue::~FutexWaitQueue() {
}

int FutexWaitQueue::Wait(uint32_t* futexPtr, Process* proc, uint32_t expected, timespec* tm) {
    int intState = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);

    // re-check under the lock -- this is what closes the lost-wakeup race.
    // nothing can mutate *futexPtr and call Wake() between this check and us joining m_waitingThreads.
    uint32_t word;
    if (!UserReadAtomic32(futexPtr, &word, proc)) {
        spinlock_release(&m_lock);
        Processor::EnableInterrupts(intState);
        return -EFAULT;
    }
    if (word != expected) {
        spinlock_release(&m_lock);
        Processor::EnableInterrupts(intState);
        return -EAGAIN;
    }

    Thread* thread = Scheduler::RemoveCurrentThread(true);
    assert(thread != nullptr);
    thread->yieldCallback = {};
    thread->sleepRemainingTime = 0; // TODO: timeout
    thread->blockedFutex = this;
    thread->wakeReason = FutexWakeReason::None;
    m_waitingThreads.pushBack(thread);
    spinlock_release(&m_lock);
    Scheduler_SaveAndYield(thread);
    // resumed here — by Wake(), a timeout, or a forced kill
    Processor::EnableInterrupts(intState);
    switch (thread->wakeReason) {
    case FutexWakeReason::Woken:
        return ESUCCESS;
    case FutexWakeReason::TimedOut:
        return -ETIMEDOUT;
    case FutexWakeReason::Interrupted:
        return -EINTR;
    default:
        assert(false);
    }
}

int FutexWaitQueue::Wake(uint32_t maxCount) {
    int intState = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);
    int woken = 0;
    while (woken < (int)maxCount && m_waitingThreads.getCount() > 0) {
        Thread* thread = m_waitingThreads.popFront();
        thread->yieldCallback = {};
        thread->sleepRemainingTime = 0;
        thread->blockedFutex = nullptr;
        thread->wakeReason = FutexWakeReason::Woken;
        assert(Scheduler::AddExistingThread(thread));
        woken++;
    }
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(intState);
    return woken;
}

void FutexWaitQueue::Remove(Thread* thread, FutexWakeReason reason) {
    spinlock_acquire(&m_lock);
    // guard: thread may have already been popped by a racing Wake() or another
    // Remove() call between when the caller decided to act and now. If so,
    // blockedFutex was already cleared and this is a no-op.
    if (thread->blockedFutex == this) {
        m_waitingThreads.remove(thread);
        thread->blockedFutex = nullptr;
        thread->wakeReason = reason;
        assert(Scheduler::AddExistingThread(thread));
    }
    spinlock_release(&m_lock);
}

FutexWaitQueue* GetOrCreateFutex(Process* proc, uint64_t virt) {
    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& tree = proc->GetFutextList();
    tree.lock();
    FutexWaitQueue* q = tree.Find(virt);
    if (q == nullptr) {
        q = new FutexWaitQueue();
        tree.Insert(virt, q);
    }
    q->refCount++;
    tree.unlock();
    return q;
}

FutexWaitQueue* GetFutexForWake(Process* proc, uint64_t virt) {
    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& tree = proc->GetFutextList();
    tree.lock();
    FutexWaitQueue* q = tree.Find(virt);
    if (q != nullptr)
        q->refCount++;
    tree.unlock();
    return q;
}

void PutFutex(Process* proc, uint64_t virt, FutexWaitQueue* q) {
    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& tree = proc->GetFutextList();
    tree.lock();
    q->refCount--;
    if (q->refCount == 0) {
        tree.Remove(virt);
        tree.unlock();
        delete q;
        return;
    }
    tree.unlock();
}

int sys_futex(int operation, uint32_t* futexPtr, uint32_t value, timespec* tm) {
    if ((operation != FUTEX_WAKE && operation != FUTEX_WAIT) || futexPtr == nullptr)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr || vmm->GetPageMapper() == nullptr)
        return -ENOSYS;

    if (!vmm->ValidateRead(futexPtr, sizeof(uint32_t)))
        return -EFAULT;

    timespec ktm = {0, 0};
    if (tm != nullptr) {
        if (!UserRead(tm, &ktm, sizeof(timespec), proc))
            return -EFAULT;
    }

    uint64_t virt = (uint64_t)futexPtr;

    int rc = ESUCCESS;
    FutexWaitQueue* q;

    switch (operation) {
    case FUTEX_WAIT:
        q = GetOrCreateFutex(proc, virt);
        rc = q->Wait(futexPtr, proc, value, &ktm);
        PutFutex(proc, virt, q);
        break;
    case FUTEX_WAKE:
        q = GetFutexForWake(proc, virt);
        if (q == nullptr)
            return 0;
        rc = q->Wake(value);
        PutFutex(proc, virt, q);
        break;
    }

    return rc;
}
