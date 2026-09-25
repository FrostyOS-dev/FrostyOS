#ifndef _SCHED_EVENT_HPP
#define _SCHED_EVENT_HPP

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>

#define PROCESS_EXIT_EVENT 1

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
    int BlockOnWaitNodes(EventWaitNode** nodes, size_t count, uint64_t timeoutMS);
    void UnregisterWaitNodes(EventWaitNode** nodes, size_t count);

    // Wrapper around the 3 above functions
    int WaitOnEvents(EventWaitNode** nodes, size_t count, uint64_t timeoutMS);
}


#endif /* _SCHED_EVENT_HPP */