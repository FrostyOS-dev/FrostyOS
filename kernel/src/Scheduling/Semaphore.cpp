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
    spinlock_acquire(&m_lock);
    while (m_waitingThreads.getCount() > 0) {
        Thread* thread = m_waitingThreads.popFront();
        assert(thread != nullptr);
        thread->yieldCallback = {};
        assert(Scheduler::AddExistingThread(thread));
    }
    spinlock_release(&m_lock);
}

void Semaphore::Wait() {
    int intState = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);
    if (m_value > 0) { // fast path: value is already > 0
        m_value--;
        spinlock_release(&m_lock);
        Processor::EnableInterrupts(intState);
        return;
    }
    while (true) {
        Scheduler::ProcessorState* state = GetCurrentProcessorState();
        if (Scheduler::isRunning() && state != nullptr && state->processor != nullptr && state->currentThread != nullptr) {
            Thread* thread = Scheduler::RemoveCurrentThread(true);
            assert(thread != nullptr);
            thread->sleepRemainingTime = 0;
            thread->yieldCallback = {};
            m_waitingThreads.pushBack(thread); // queue waiter before yielding to avoid lost wakeups
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
    spinlock_acquire(&m_lock);
    if (m_value < m_maxValue) {
        m_value++;
        if (Scheduler::isRunning() && m_waitingThreads.getCount() > 0) {
            Thread* thread = m_waitingThreads.popFront();
            thread->yieldCallback = {};
            thread->sleepRemainingTime = 0;
            assert(Scheduler::AddExistingThread(thread));
        }
    }
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(intState);
}
