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

#include "Event.hpp"
#include "Scheduler.hpp"
#include "Thread.hpp"

#include <errno.h>

#include <HAL/Processor.hpp>

EventWaitQueue::EventWaitQueue() : m_head(nullptr), m_tail(nullptr), m_lock(SPINLOCK_DEFAULT_VALUE) {
}

EventWaitQueue::~EventWaitQueue() {
}

void EventWaitQueue::AddListener(EventWaitNode* node) {
    int state = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);
    
    node->queue = this;
    node->prev = m_tail;
    node->next = nullptr;
    
    if (m_tail != nullptr)
        m_tail->next = node;
    else
        m_head = node;
        
    m_tail = node;
    
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(state);
}

void EventWaitQueue::RemoveListener(EventWaitNode* node) {
    int state = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);
    
    if (node->queue == this) {
        if (node->prev != nullptr)
            node->prev->next = node->next;
        else
            m_head = node->next;
        
        if (node->next != nullptr)
            node->next->prev = node->prev;
        else
            m_tail = node->prev;
        
        node->queue = nullptr;
    }
    
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(state);
}

void EventWaitQueue::Trigger(uint32_t events, void* data) {
    int state = Processor::DisableInterrupts();
    spinlock_acquire(&m_lock);
    
    EventWaitNode* current = m_head;
    while (current != nullptr) {
        if ((current->requestedEvents & events) != 0) {
            current->triggeredEvents |= (current->requestedEvents & events);
            current->eventData = data;
            
            Thread* thread = current->thread;
            
            spinlock_acquire(&thread->eventLock);
            thread->WakeFromEventWait();
            spinlock_release(&thread->eventLock);
        }
        current = current->next;
    }
    
    spinlock_release(&m_lock);
    Processor::EnableInterrupts(state);
}

namespace Event {
    void RegisterWaitNodes(EventWaitNode** nodes, size_t count) {
        int intState = Processor::DisableInterrupts();
        Scheduler::ProcessorState* state = GetCurrentProcessorState();
        Thread* currentThread = state->currentThread;

        spinlock_acquire(&currentThread->eventLock);
        currentThread->activeEventNodes = nullptr;

        for (size_t i = 0; i < count; i++) {
            nodes[i]->thread = currentThread;
            nodes[i]->triggeredEvents = 0;

            nodes[i]->threadNext = currentThread->activeEventNodes;
            currentThread->activeEventNodes = nodes[i];
            spinlock_release(&currentThread->eventLock);

            if (nodes[i]->queue != nullptr)
                nodes[i]->queue->AddListener(nodes[i]);

            spinlock_acquire(&currentThread->eventLock);
        }
        spinlock_release(&currentThread->eventLock);
        Processor::EnableInterrupts(intState);
    }

    int BlockOnWaitNodes(EventWaitNode** nodes, size_t count, uint64_t timeoutMS, bool killOnly) {
        int intState = Processor::DisableInterrupts();
        Scheduler::ProcessorState* state = GetCurrentProcessorState();
        Thread* currentThread = state->currentThread;

        spinlock_acquire(&currentThread->eventLock);

        int triggeredCount = 0;
        for (size_t i = 0; i < count; i++) {
            if (nodes[i]->triggeredEvents != 0)
                triggeredCount++;
        }

        if (triggeredCount > 0 || (timeoutMS == 0 && count > 0)) {
            spinlock_release(&currentThread->eventLock);
            Processor::EnableInterrupts(intState);
            return triggeredCount;
        }

        auto signalled = [&]() { return killOnly ? currentThread->HasPendingKill() : currentThread->HasPendingUnblockedSignals(); };

        if (signalled()) {
            spinlock_release(&currentThread->eventLock);
            Processor::EnableInterrupts(intState);
            return -EINTR;
        }

        currentThread->eventWaitActive = true;
        __atomic_store_n(&currentThread->blocked, true, __ATOMIC_RELEASE);

        Scheduler::RemoveCurrentThread(true);
        currentThread->sleepRemainingTime = timeoutMS;
        currentThread->yieldCallback = {
            [](Thread* t, void* data) {
                spinlock_release(static_cast<spinlock_t*>(data));
            }, &currentThread->eventLock
        };

        Scheduler_SaveAndYield(currentThread);

        spinlock_acquire(&currentThread->eventLock);

        currentThread->eventWaitActive = false;

        triggeredCount = 0;
        for (size_t i = 0; i < count; i++) {
            if (nodes[i]->triggeredEvents != 0)
                triggeredCount++;
        }

        spinlock_release(&currentThread->eventLock);
        Processor::EnableInterrupts(intState);

        if (triggeredCount > 0)
            return triggeredCount;

        if (signalled())
            return -EINTR;

        return -ETIMEDOUT;
    }

    void UnregisterWaitNodes(EventWaitNode** nodes, size_t count) {
        int intState = Processor::DisableInterrupts();
        Scheduler::ProcessorState* state = GetCurrentProcessorState();
        Thread* currentThread = state->currentThread;

        for (size_t i = 0; i < count; i++) {
            if (nodes[i]->queue != nullptr)
            nodes[i]->queue->RemoveListener(nodes[i]);
        }
    
        spinlock_acquire(&currentThread->eventLock);
        currentThread->eventWaitActive = false;
        currentThread->activeEventNodes = nullptr;
        spinlock_release(&currentThread->eventLock);
        Processor::EnableInterrupts(intState);
    }

    int WaitOnEvents(EventWaitNode** nodes, size_t count, uint64_t timeoutMS) {
        RegisterWaitNodes(nodes, count);
        int rc = BlockOnWaitNodes(nodes, count, timeoutMS);
        UnregisterWaitNodes(nodes, count);
        return rc;
    }
}