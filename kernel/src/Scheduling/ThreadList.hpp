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

#ifndef _THREAD_LIST_HPP
#define _THREAD_LIST_HPP

#include <stdint.h>

#include <spinlock.h>

class Thread;

struct ThreadListItemInternalData {
    Thread* previous;
    Thread* next;
};

class [[gnu::packed]] ThreadList {
public:
    ThreadList();
    ~ThreadList();

    void pushBack(Thread* thread);
    Thread* popFront();

    void remove(Thread* thread);

    Thread* getHead() const;
    Thread* getTail() const;

    uint64_t getCount() const;

    void lock() const;
    void unlock() const;

private:
    Thread* m_Head;
    Thread* m_Tail;
    uint64_t m_Count;
    mutable spinlock_t m_Lock;
};


#endif /* _THREAD_LIST_HPP */