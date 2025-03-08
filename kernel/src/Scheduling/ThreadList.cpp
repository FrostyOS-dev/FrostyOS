/*
Copyright (©) 2025  Frosty515

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

ThreadList::ThreadList() : m_Head(nullptr), m_Tail(nullptr), m_Count(0) {
}

ThreadList::~ThreadList() {
}

void ThreadList::pushBack(Thread* thread) {
    if (m_Head == nullptr) {
        m_Head = thread;
        m_Tail = thread;
        m_Count++;
        return;
    }

    m_Tail->GetThreadListData().next = thread;
    thread->GetThreadListData().previous = m_Tail;
    m_Tail = thread;
    m_Count++;
}

Thread* ThreadList::popFront() {
    if (m_Head == nullptr)
        return nullptr;

    Thread* temp = m_Head;
    m_Head = m_Head->GetThreadListData().next;
    if (m_Head != nullptr)
        m_Head->GetThreadListData().previous = nullptr;
    else
        m_Tail = nullptr;
    m_Count--;
    return temp;
}

void ThreadList::remove(Thread* thread) {
    if (thread->GetThreadListData().previous != nullptr)
        thread->GetThreadListData().previous->GetThreadListData().next = thread->GetThreadListData().next;
    else
        m_Head = thread->GetThreadListData().next;

    if (thread->GetThreadListData().next != nullptr)
        thread->GetThreadListData().next->GetThreadListData().previous = thread->GetThreadListData().previous;
    else
        m_Tail = thread->GetThreadListData().previous;

    m_Count--;
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

void ThreadList::lock() const {
    spinlock_acquire(&m_Lock);
}

void ThreadList::unlock() const {
    spinlock_release(&m_Lock);
}
