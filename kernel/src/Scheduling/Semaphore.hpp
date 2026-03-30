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

#ifndef _SEMAPHORE_HPP
#define _SEMAPHORE_HPP

#include <stdint.h>

#include "ThreadList.hpp"

class Thread;

class Semaphore {
public:
    Semaphore(uint64_t value = 0, uint64_t maxValue = UINT64_MAX);
    ~Semaphore();

    void Wait();
    void Signal();

private:
    uint64_t m_value;
    uint64_t m_maxValue;
    ThreadList m_waitingThreads;
    spinlock_t m_lock;
};

#endif /* _SEMAPHORE_HPP */