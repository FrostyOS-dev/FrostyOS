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

#include "InputClient.hpp"
#include "InputEvent.hpp"
#include "spinlock.h"

#include <errno.h>
#include <string.h>

#include <frostyos/asm/input.h>

#include <Scheduling/Event.hpp>

#include <SystemCalls/File.hpp>
#include <SystemCalls/SystemCall.hpp>

namespace {
    constexpr int64_t NS_PER_SEC = 1'000'000'000;
    constexpr size_t QUEUE_MASK = INPUT_CLIENT_QUEUE_EVENTS - 1;

    void Stamp(uint64_t monoNS, int clock, int64_t rtOffset, int64_t* sec, int64_t* nsec) {
        int64_t ns = static_cast<int64_t>(monoNS);
        if (clock == FOSI_CLOCK_REALTIME)
            ns += rtOffset;
        int64_t s = ns / NS_PER_SEC;
        int64_t n = ns % NS_PER_SEC;
        if (n < 0) {
            n += NS_PER_SEC;
            s--;
        }
        *sec = s;
        *nsec = n;
    }
}

InputClient::InputClient() : m_source(nullptr), m_prev(nullptr), m_next(nullptr), m_view(), m_claimed(false), m_ownerPID(-1), m_clock(FOSI_CLOCK_REALTIME), m_head(0), m_tail(0), m_hangup(false), m_waitQueue() {
    spinlock_init(&m_qLock);
}

InputClient::~InputClient() {

}

int InputClient::Read(void* out, size_t size, int flags, size_t* bytesRead) {
    *bytesRead = 0;
    if (size < sizeof(FOSI_Event))
        return -ERANGE;

    size_t maxEvents = size / sizeof(FOSI_Event);

    while (true) {
        spinlock_acquire_int(&m_qLock);
        size_t avail = m_head - m_tail;
        if (avail > 0) {
            size_t n = avail < maxEvents ? avail : maxEvents;
            for (size_t i = 0; i < n; i++)
                memcpy(static_cast<uint8_t*>(out) + i * sizeof(FOSI_Event), &m_buf[(m_tail + i) & QUEUE_MASK], sizeof(FOSI_Event));
            m_tail += n;
            spinlock_release_int(&m_qLock);
            *bytesRead = n * sizeof(FOSI_Event);
            return ESUCCESS;
        }
        bool hangup = m_hangup;
        spinlock_release_int(&m_qLock);

        if (hangup)
            return -ENODEV;
        if ((flags & O_NONBLOCK) > 0)
            return -EAGAIN;

        EventWaitNode node;
        node.queue = &m_waitQueue;
        node.requestedEvents = POLLIN | POLLHUP;
        EventWaitNode* nodes[] = {&node};
        ::Event::RegisterWaitNodes(nodes, 1);

        if (HasDataOrHangup()) {
            ::Event::UnregisterWaitNodes(nodes, 1);
            continue;
        }

        int rc = ::Event::BlockOnWaitNodes(nodes, 1, UINT64_MAX);
        ::Event::UnregisterWaitNodes(nodes, 1);

        if (rc < 0)
            return rc;
    }
}

uint32_t InputClient::PollReady(uint32_t events) {
    uint32_t e = 0;
    spinlock_acquire_int(&m_qLock);
    if (m_head != m_tail)
        e |= POLLIN | POLLRDNORM;
    if (m_hangup)
        e |= POLLHUP;
    spinlock_release_int(&m_qLock);
    return (e & events) | (e & POLLHUP); // HUP is always reported
}

void InputClient::Deliver(const Input::Frame& frame, int64_t rtOffset) {
    Input::Frame f;
    if (!Input::FilterFrame(frame, m_view, &f))
        return;
    if (TryCommit(f, rtOffset))
        return;

    Resync(frame.monoNS, rtOffset);
    if (Input::FilterFrame(frame, m_view, &f))
        TryCommit(f, rtOffset);
}

void InputClient::ReleaseAllKeys(uint64_t monoNS, int64_t rtOffset) {
    bool overflowed = false;
    Input::ForEachKeyReleaseFrame(m_view, m_source != nullptr ? m_source->id : 0, monoNS, [&](const Input::Frame& f) {
        if (!overflowed && !TryCommit(f, rtOffset))
            overflowed = true;
    });
    if (overflowed)
        Resync(monoNS, rtOffset);
}

void InputClient::Resync(uint64_t monoNS, int64_t rtOffset) {
    int64_t sec, nsec;
    Stamp(monoNS, m_clock, rtOffset, &sec, &nsec);

    FOSI_Event ev[2];
    ev[0] = {sec, nsec, FOSI_ET_SYN, FOSI_SYN_DROPPED, 0};
    ev[1] = {sec, nsec, FOSI_ET_SYN, FOSI_SYN_REPORT, 0};

    spinlock_acquire_int(&m_qLock);
    m_tail = m_head;
    for (size_t i = 0; i < 2; i++)
        m_buf[(m_head + i) & QUEUE_MASK] = ev[i];
    m_head += 2;
    spinlock_release_int(&m_qLock);

    m_view.Reset();
    Wake(POLLIN | POLLRDNORM);
}

void InputClient::SetClock(int clock, uint64_t monoNS, int64_t rtOffset) {
    if (clock == m_clock)
        return;
    m_clock = clock;
    Resync(monoNS, rtOffset);
}

void InputClient::Hangup() {
    spinlock_acquire_int(&m_qLock);
    m_hangup = true;
    spinlock_release_int(&m_qLock);
    Wake(POLLHUP | POLLIN | POLLRDNORM);
}

bool InputClient::TryCommit(const Input::Frame& f, int64_t rtOffset) {
    int64_t sec, nsec;
    Stamp(f.monoNS, m_clock, rtOffset, &sec, &nsec);

    FOSI_Event ev[Input::MAX_EVENTS_PER_FRAME + 1];
    size_t n = 0;
    for (uint32_t i = 0; i < f.count; i++) {
        ev[n].tv_sec = sec;
        ev[n].tv_nsec = nsec;
        ev[n].type = f.events[i].type;
        ev[n].code = f.events[i].code;
        ev[n].value = f.events[i].value;
        n++;
    }
    ev[n].tv_sec = sec;
    ev[n].tv_nsec = nsec;
    ev[n].type = FOSI_ET_SYN;
    ev[n].code = FOSI_SYN_REPORT;
    ev[n].value = 0;
    n++;

    return PushAtomic(ev, n);
}

bool InputClient::PushAtomic(const FOSI_Event* ev, size_t n) {
    spinlock_acquire_int(&m_qLock);
    if (INPUT_CLIENT_QUEUE_EVENTS - (m_head - m_tail) < n) {
        spinlock_release_int(&m_qLock);
        return false;
    }
    for (size_t i = 0; i < n; i++)
        m_buf[(m_head + i) & QUEUE_MASK] = ev[i];
    m_head += n;
    spinlock_release_int(&m_qLock);

    Wake(POLLIN | POLLRDNORM);
    return true;
}

bool InputClient::HasDataOrHangup() {
    spinlock_acquire_int(&m_qLock);
    bool r = m_head != m_tail || m_hangup;
    spinlock_release_int(&m_qLock);
    return r;

}

void InputClient::Wake(uint32_t events) {
    m_waitQueue.Trigger(events);
}
