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

#include "Pipe.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <Scheduling/Event.hpp>
#include <Scheduling/Mutex.hpp>
#include <Scheduling/Thread.hpp>

#include <SystemCalls/File.hpp>

Pipe::Pipe() : m_buf(nullptr), m_head(0), m_count(0), m_readOpen(true), m_writeOpen(true), m_refCount(2) {

}

Pipe::~Pipe() {
    if (m_buf != nullptr)
        delete[] m_buf;
}

bool Pipe::Init() {
    m_buf = new uint8_t[PIPE_CAPACITY];
    return m_buf != nullptr;
}

int Pipe::Read(void* out, size_t count, size_t* done, bool nonBlock) {
    *done = 0;
    return WaitLoop(POLLIN, nonBlock, [&]() -> int {
        m_lock.Lock();
        if (m_count > 0) {
            *done = CopyOut(static_cast<uint8_t*>(out), count);
            m_lock.Unlock();
            m_queue.Trigger(POLLOUT);
            return 1;
        }
        bool eof = !m_writeOpen;
        m_lock.Unlock();
        return eof ? 1 : 0;
    });
}

int Pipe::Write(const void* in, size_t count, size_t* done, bool nonBlock) {
    *done = 0;
    bool atomic = count <= PIPE_BUF;

    int rc = WaitLoop(POLLOUT, nonBlock, [&]() -> int {
        m_lock.Lock();
        if (!m_readOpen) {
            m_lock.Unlock();
            return -EPIPE;
        }
        size_t space = PIPE_CAPACITY - m_count;
        size_t remaining = count - *done;
        if (space >= (atomic ? remaining : 1)) {
            *done += CopyIn(static_cast<const uint8_t*>(in) + *done, MIN(space, remaining));
            m_lock.Unlock();
            m_queue.Trigger(POLLIN);
            return *done == count ? 1 : 0;
        }
        m_lock.Unlock();
        return 0;
    });

    if (rc == -EPIPE)
        Thread::GetCurrentThread()->RaiseSignal(SIGPIPE);
    if (rc < 0 && *done > 0)
        rc = 0;
    return rc;
}

void Pipe::CloseEnd(bool writeEnd) {
    m_lock.Lock();
    if (writeEnd)
        m_writeOpen = false;
    else
        m_readOpen = false;
    m_lock.Unlock();
    m_queue.Trigger(POLLIN | POLLOUT | POLLHUP | POLLERR);
}

uint32_t Pipe::PollReady(bool writeEnd, uint32_t events) {
    uint32_t r = 0;
    m_lock.Lock();
    if (!writeEnd) {
        if (m_count > 0)
            r |= POLLIN | POLLRDNORM;
        if (!m_writeOpen)
            r |= POLLHUP;
    } else {
        if (!m_readOpen)
            r |= POLLERR;
        else if (PIPE_CAPACITY - m_count >= PIPE_BUF)
            r |= POLLOUT | POLLWRNORM;
    }
    m_lock.Unlock();
    return r;
}

EventWaitQueue* Pipe::GetPollQueue() {
    return &m_queue;
}

void Pipe::Unref() {
    if (__atomic_sub_fetch(&m_refCount, 1, __ATOMIC_ACQ_REL) == 0)
        delete this;
}

template <typename F>
int Pipe::WaitLoop(uint32_t events, bool nonBlock, F tryOp) {
    EventWaitNode node{};
    node.queue = &m_queue;
    node.requestedEvents = events | POLLERR | POLLHUP;
    EventWaitNode* nodes[1] = { &node };

    Event::RegisterWaitNodes(nodes, 1);
    int rc;
    while (true) {
        node.triggeredEvents = 0;
        rc = tryOp();
        if (rc != 0)
            break;
        if (nonBlock) {
            rc = -EAGAIN;
            break;
        }
        if (Event::BlockOnWaitNodes(nodes, 1, UINT64_MAX) == -EINTR) {
            rc = -EINTR;
            break;
        }
    }
    Event::UnregisterWaitNodes(nodes, 1);
    return rc > 0 ? 0 : rc;
}

size_t Pipe::CopyIn(const uint8_t* src, size_t n) {
    size_t tail = (m_head + m_count) % PIPE_CAPACITY;
    size_t first = MIN(n, (size_t)PIPE_CAPACITY - tail);
    memcpy(m_buf + tail, src, first);
    memcpy(m_buf, src + first, n - first);
    m_count += n;
    return n;
}

size_t Pipe::CopyOut(uint8_t* dst, size_t n) {
    n = MIN(n, m_count);
    size_t first = MIN(n, (size_t)PIPE_CAPACITY - m_head);
    memcpy(dst, m_buf + m_head, first);
    memcpy(dst + first, m_buf, n - first);
    m_head = (m_head + n) % PIPE_CAPACITY;
    m_count -= n;
    return n;
}
