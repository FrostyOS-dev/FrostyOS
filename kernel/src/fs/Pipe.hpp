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

#ifndef _PIPE_HPP
#define _PIPE_HPP

#include <stddef.h>
#include <stdint.h>

#include <Scheduling/Event.hpp>
#include <Scheduling/Mutex.hpp>

#define PIPE_CAPACITY 65536
#define PIPE_BUF 4096

class Pipe {
public:
    Pipe();
    ~Pipe();

    bool Init();

    int Read(void* out, size_t count, size_t* done, bool nonBlock);
    int Write(const void* in, size_t count, size_t* done, bool nonBlock);

    void CloseEnd(bool writeEnd);

    uint32_t PollReady(bool writeEnd, uint32_t events);
    EventWaitQueue* GetPollQueue();

    void Unref();

private:
    template <typename F>
    int WaitLoop(uint32_t events, bool nonBlock, F tryOp);

    size_t CopyIn(const uint8_t* src, size_t n);
    size_t CopyOut(uint8_t* src, size_t n);

    Mutex m_lock;
    EventWaitQueue m_queue;
    uint8_t* m_buf;
    size_t m_head;
    size_t m_count;
    bool m_readOpen;
    bool m_writeOpen;
    uint64_t m_refCount;
};

#endif /* _PIPE_HPP */