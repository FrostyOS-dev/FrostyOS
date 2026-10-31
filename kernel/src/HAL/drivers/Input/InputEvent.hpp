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

#ifndef _HAL_INPUT_EVENT_HPP
#define _HAL_INPUT_EVENT_HPP

#include <Function.hpp>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/Buffer.hpp>

#include <fs/DevTempFS/DevTempFS.hpp>

#include <Scheduling/Mutex.hpp>
#include <Scheduling/Semaphore.hpp>

#include "Key.hpp"

#define MAX_INPUT_EVENTS 512

class InputDevice;

namespace Input {

    typedef uint64_t EventSourceID;

    enum class EventType : uint64_t {
        KeyboardKey = 1
    };

    enum class EventSourceType : uint64_t {
        Keyboard = 1
    };

    struct Event {
        EventType type;
        EventSourceID source;
        union {
            struct KeyboardEvent {
                Key key;
                bool released;
            } keyboardEvent;
        } eventData;
    };

    typedef bool (*EventHandlerFunc)(const Event* e, void* ctx); // true = consumed

    class EventListener {
    public:
        EventListener(EventHandlerFunc fn, void* ctx) : m_fn(fn), m_ctx(ctx) {}
        EventListener(const EventListener&) = delete;
        EventListener& operator=(const EventListener&) = delete;
    private:
        friend class EventManager;

        EventHandlerFunc m_fn;
        void* m_ctx;
        EventListener* m_prev = nullptr;
        EventListener* m_next = nullptr;
        EventSourceID m_sourceID = 0;
        bool m_registered = false;
    };

    class EventManager {
    public:
        EventManager();
        ~EventManager();

        bool Init();
        int FullInit(); // initialise nodes in the devfs

        EventSourceID RegisterEventSource(EventSourceType type, InputDevice* device);
        InputDevice* GetDevice(EventSourceID id, EventSourceType* type = nullptr);

        // For the following 2 functions:
        // The list of event sources and the event source itself are locked during callbacks, so no EventManager operations can be called.
        
        void EnumerateEventSources(FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback);
        void EnumerateEventSources(uint64_t types, FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback);

        void RaiseEvent(const Event& e);

        bool RegisterEventHandler(EventSourceID id, EventListener& l);
        bool UnregisterEventHandler(EventListener& l);

        [[noreturn]] void WorkerThreadMain();

    private:
        struct EventSource {
            EventSourceID id;
            EventSourceType type;
            InputDevice* device;
            FS::DevTempFSVNode* vnode = nullptr;
            Mutex lock; // protects only the listener list
            EventListener* head = nullptr;
            EventListener* tail = nullptr;
        };

        EventSource* FindSource(EventSourceID id); 

        uint64_t m_nextEventSourceID;
        AVLTree::wAVLTree<EventSourceID, EventSource*> m_eventSources;

        MPSCRingBuffer<Event, MAX_INPUT_EVENTS> m_pendingEvents;
        Semaphore m_eventSemaphore;

        Thread* m_workerThread;

        FS::DevTempFSVNode* m_rootVNode;
    };

    bool InitTTYEvents();

    extern EventManager* g_EventManager;
}

#endif /* _HAL_INPUT_EVENT_HPP */