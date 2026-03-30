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

#include "Scheduler.hpp"
#include "Semaphore.hpp"
#include "ThreadList.hpp"

#include <spinlock.h>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>
#include <HAL/Time.hpp>

Semaphore::Semaphore(uint64_t value, uint64_t maxValue) : m_value(value), m_maxValue(maxValue), m_waitingThreads(), m_lock(SPINLOCK_DEFAULT_VALUE) {

}

Semaphore::~Semaphore() {

}

void Semaphore::Wait() {
    int intState = Processor::DisableInterrupts();
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    if (state == nullptr || state->processor == nullptr) {
        Processor::EnableInterrupts(intState);
        return;
    }
    spinlock_acquire(&m_lock);
    dbgprintf("waiting semaphore on CPU %lu, thread %ld, m_value = %lx\n", state->id, state->currentThread == nullptr ? -1 : state->currentThread->GetTID(), m_value);
    if (m_value > 0) { // fast path: value is already > 0
        m_value--;
        spinlock_release(&m_lock);
        Processor::EnableInterrupts(intState);
        return;
    }
    while (true) {
        if (Scheduler::isRunning() && state->currentThread != nullptr) {
            Thread* thread = Scheduler::RemoveCurrentThread();
            assert(thread != nullptr);
            m_waitingThreads.pushBack(thread); // go to the back of the queue
            spinlock_release(&m_lock);
            Scheduler_SaveAndYield(thread);
            spinlock_acquire(&m_lock);
        } else {
            spinlock_release(&m_lock);
            PAUSE();
            spinlock_acquire(&m_lock);
        }
        if (m_value > 0) {
            m_value--;
            spinlock_release(&m_lock);
            Processor::EnableInterrupts(intState);
            return;
        }
    }
}

void Semaphore::Signal() {
    int intState = Processor::DisableInterrupts();
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    if (state == nullptr || state->processor == nullptr) {
        Processor::EnableInterrupts(intState);
        return;
    }
    dbgprintf("signalling semaphore on CPU %lu, thread %ld\n", state->id, state->currentThread == nullptr ? -1 : state->currentThread->GetTID());
    spinlock_acquire(&m_lock);
    if (m_value < m_maxValue) {
        m_value++;
        if (Scheduler::isRunning() && m_waitingThreads.getCount() > 0) {
            Thread* thread = m_waitingThreads.popFront();
            assert(Scheduler::AddExistingThread(thread));
        }
    }
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(intState);
}
