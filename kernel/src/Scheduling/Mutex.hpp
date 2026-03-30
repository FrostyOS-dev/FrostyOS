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

#ifndef _MUTEX_HPP
#define _MUTEX_HPP

#include "Semaphore.hpp"

class Mutex {
public:
    Mutex() : m_semaphore(1, 1) {}
    ~Mutex() {}

    inline void Lock() {
        m_semaphore.Wait();
    }

    inline void Unlock() {
        m_semaphore.Signal();
    }

private:
    Semaphore m_semaphore;
};

#endif /* _MUTEX_HPP */