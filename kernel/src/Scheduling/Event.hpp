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

#ifndef _SCHED_EVENT_HPP
#define _SCHED_EVENT_HPP

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>

#define PROCESS_EXIT_EVENT 0x10000
#define PROCESS_CONTINUE_EVENT 0x20000
#define POLLIN 0x1
#define POLLPRI 0x2
#define POLLOUT 0x4
#define POLLERR 0x8
#define POLLHUP 0x10
#define POLLNVAL 0x20
#define POLLRDNORM 0x40
#define POLLRDBAND 0x80
#define POLLWRNORM 0x100
#define POLLRDHUP 0x2000

class Thread;
class EventWaitQueue;

struct EventWaitNode {
    Thread* thread;
    EventWaitQueue* queue;
    uint32_t requestedEvents;
    uint32_t triggeredEvents;
    void* eventData;

    // Intrusive linked list of nodes
    EventWaitNode* prev;
    EventWaitNode* next;

    // Intrusive linked list for the Thread
    EventWaitNode* threadNext;
};

class EventWaitQueue {
public:
    EventWaitQueue();
    ~EventWaitQueue();

    void AddListener(EventWaitNode* node);
    void RemoveListener(EventWaitNode* node);

    void Trigger(uint32_t events, void* data = nullptr);

private:
    EventWaitNode* m_head;
    EventWaitNode* m_tail;
    spinlock_t m_lock;
};

namespace Event {
    void RegisterWaitNodes(EventWaitNode** nodes, size_t count);
    int BlockOnWaitNodes(EventWaitNode** nodes, size_t count, uint64_t timeoutMS, bool killOnly = false);
    void UnregisterWaitNodes(EventWaitNode** nodes, size_t count);

    // Wrapper around the 3 above functions
    int WaitOnEvents(EventWaitNode** nodes, size_t count, uint64_t timeoutMS);
}


#endif /* _SCHED_EVENT_HPP */