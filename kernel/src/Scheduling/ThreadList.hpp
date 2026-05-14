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

#ifndef _THREAD_LIST_HPP
#define _THREAD_LIST_HPP

#include <stdint.h>

#include <spinlock.h>

class Thread;

struct ThreadListItemInternalData {
    Thread* previous;
    Thread* next;
};

class ThreadList {
public:
    enum class IteratorDecision {
        Break,
        Continue,
        Delete_Break,
        Delete
    };

    ThreadList();
    ~ThreadList();

    void pushBack(Thread* thread);
    Thread* popFront();

    void remove(Thread* thread);

    Thread* getHead() const;
    Thread* getTail() const;

    uint64_t getCount() const;

    void Enumerate(IteratorDecision (*func)(Thread*, void*), void* data); // Deletion of the current and previous threads is allowed during the provided function.
    void EnumerateConst(IteratorDecision (*func)(Thread*, void*), void* data) const; // Does not support deletion, will just break/continue depending on type

    void lock() const;
    void unlock() const;

    virtual bool IsProcList() const;

protected:
    virtual ThreadListItemInternalData& GetDataFromThread(Thread* thread) const;

private:
    Thread* m_Head;
    Thread* m_Tail;
    uint64_t m_Count;
    mutable spinlock_t m_Lock;
    mutable int m_intState;
};

class ProcThreadList : public ThreadList {
protected:
    ThreadListItemInternalData& GetDataFromThread(Thread* thread) const override;
    bool IsProcList() const override;
};


#endif /* _THREAD_LIST_HPP */