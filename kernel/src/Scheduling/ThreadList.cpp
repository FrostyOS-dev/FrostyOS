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

#include "ThreadList.hpp"
#include "Thread.hpp"

#include <spinlock.h>
#include <stdint.h>

#include <HAL/Processor.hpp>

ThreadList::ThreadList() : m_Head(nullptr), m_Tail(nullptr), m_Count(0) {
}

ThreadList::~ThreadList() {
}

void ThreadList::pushBack(Thread* thread) {
    assert(thread != nullptr);
    ThreadListItemInternalData& data = GetDataFromThread(thread);
    // A thread can only be linked into one scheduler list at a time.
    assert(!(data.previous != nullptr || data.next != nullptr || thread == m_Head || thread == m_Tail));
    if (m_Head == nullptr) {
        m_Head = thread;
        m_Tail = thread;
        m_Count++;
        data = {nullptr, nullptr};
        if (IsProcList())
            thread->SetInProcList(true);
        else
            thread->SetInSchedList(true);
        return;
    }

    GetDataFromThread(m_Tail).next = thread;
    data.previous = m_Tail;
    data.next = nullptr;
    m_Tail = thread;
    m_Count++;
    if (IsProcList())
        thread->SetInProcList(true);
    else
        thread->SetInSchedList(true);
}

Thread* ThreadList::popFront() {
    if (m_Head == nullptr)
        return nullptr;

    Thread* temp = m_Head;
    m_Head = GetDataFromThread(m_Head).next;
    if (m_Head != nullptr)
        GetDataFromThread(m_Head).previous = nullptr;
    else
        m_Tail = nullptr;
    m_Count--;
    GetDataFromThread(temp) = {nullptr, nullptr};
    if (IsProcList())
        temp->SetInProcList(false);
    else
        temp->SetInSchedList(false);
    return temp;
}

void ThreadList::remove(Thread* thread) {
    if (thread == nullptr)
        return;
    ThreadListItemInternalData& data = GetDataFromThread(thread);
    // Ignore invalid removals; they would corrupt head/tail/count and lose unrelated threads.
    assert((data.previous != nullptr || m_Head == thread) && (data.next != nullptr || m_Tail == thread) && (data.previous == nullptr || GetDataFromThread(data.previous).next == thread) && (data.next == nullptr || GetDataFromThread(data.next).previous == thread));
    if (data.previous != nullptr) {
        Thread* temp = data.previous;
        GetDataFromThread(temp).next = data.next;
    } else
        m_Head = data.next;

    if (data.next != nullptr) {
        Thread* temp = data.next;
        GetDataFromThread(temp).previous = data.previous;
    } else
        m_Tail = data.previous;

    data = {nullptr, nullptr};
    m_Count--;
    if (IsProcList())
        thread->SetInProcList(false);
    else
        thread->SetInSchedList(false);
}

Thread* ThreadList::getHead() const {
    return m_Head;
}

Thread* ThreadList::getTail() const {
    return m_Tail;
}

uint64_t ThreadList::getCount() const {
    return m_Count;
}

void ThreadList::Enumerate(IteratorDecision (*func)(Thread*, void*), void* data) {
    Thread* thread = m_Head;
    uint64_t originalCount = m_Count;
    for (uint64_t i = 0; i < originalCount; i++) {
        if (thread == nullptr)
            break;
        Thread* next = GetDataFromThread(thread).next;
        switch (func(thread, data)) {
        case IteratorDecision::Break:
            return;
        case IteratorDecision::Continue:
            break;
        case IteratorDecision::Delete_Break:
            remove(thread);
            return;
        case IteratorDecision::Delete: {
            remove(thread);
            thread = next;
            continue;
        }
        }

        thread = next;
    }
}

void ThreadList::EnumerateConst(IteratorDecision (*func)(Thread*, void*), void* data) const {
    Thread* thread = m_Head;
    for (uint64_t i = 0; i < m_Count; i++) {
        if (thread == nullptr)
            break;
        switch (func(thread, data)) {
        case IteratorDecision::Break:
        case IteratorDecision::Delete_Break:
            return;
        case IteratorDecision::Continue:
        case IteratorDecision::Delete:
            break;
        }

        thread = GetDataFromThread(thread).next;
    }
}

void ThreadList::lock() const {
    int intState = Processor::DisableInterrupts();
    spinlock_acquire(&m_Lock);
    m_intState = intState; // only access member variable while lock is held
}

void ThreadList::unlock() const {
    int intState = m_intState;
    spinlock_release(&m_Lock); // only access member variable while lock is held
    Processor::EnableInterrupts(intState);
}

ThreadListItemInternalData& ThreadList::GetDataFromThread(Thread* thread) const {
    return thread->GetThreadListData();
}


ThreadListItemInternalData& ProcThreadList::GetDataFromThread(Thread* thread) const {
    return thread->GetProcThreadListData();
}

bool ProcThreadList::IsProcList() const {
    return true;
}

bool ThreadList::IsProcList() const {
    return false;
}
