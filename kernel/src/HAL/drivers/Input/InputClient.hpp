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

#ifndef _DRIVERS_INPUT_CLIENT_HPP
#define _DRIVERS_INPUT_CLIENT_HPP

#include <stddef.h>
#include <stdint.h>
#include <spinlock.h>

#include <frostyos/asm/input.h>

#include <Scheduling/Event.hpp>

#include "InputEvent.hpp"

#define INPUT_CLIENT_QUEUE_EVENTS 512

class InputClient {
public:
    InputClient();
    ~InputClient();

    InputClient(const InputClient&) = delete;
    InputClient& operator=(const InputClient&) = delete;

    int Read(void* out, size_t size, int flags, size_t* bytesRead);

    EventWaitQueue* GetWaitQueue() { return &m_waitQueue; }
    uint32_t PollReady(uint32_t events);

    bool IsClaimed() const { return m_claimed; }

private:
    friend class Input::EventManager;

    void Deliver(const Input::Frame& frame, int64_t rtOffset);
    void ReleaseAllKeys(uint64_t monoNS, int64_t rtOffset);
    void Resync(uint64_t monoNS, int64_t rtOffset);
    void SetClock(int clock, uint64_t monoNS, int64_t rtOffset);
    void Hangup();

    bool TryCommit(const Input::Frame& f, int64_t rtOffset);
    bool PushAtomic(const FOSI_Event* ev, size_t n);
    bool HasDataOrHangup();
    void Wake(uint32_t events);

    Input::EventSource* m_source;
    InputClient* m_prev;
    InputClient* m_next;
    Input::KeyView m_view;
    bool m_claimed;
    int64_t m_ownerPID;
    int m_clock;

    spinlock_t m_qLock;
    size_t m_head;
    size_t m_tail;
    bool m_hangup;
    FOSI_Event m_buf[INPUT_CLIENT_QUEUE_EVENTS];

    EventWaitQueue m_waitQueue;
};

#endif /* _DRIVERS_INPUT_CLIENT_HPP */