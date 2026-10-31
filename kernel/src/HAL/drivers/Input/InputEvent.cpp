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

#include "InputDevice.hpp"
#include "InputEvent.hpp"

#include "../DeviceManager.hpp"

#include <Function.hpp>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <fs/VFS.hpp>

#include <Scheduling/Thread.hpp>

#include <tty/Input/KBDInputSource.hpp>

#include <tty/TTY.hpp>

namespace Input {

    [[noreturn]] void EventManager_WorkThreadMain(void* data) {
        static_cast<EventManager*>(data)->WorkerThreadMain();
    }

    EventManager::EventManager() : m_nextEventSourceID(0), m_eventSources(), m_pendingEvents(), m_eventSemaphore(), m_workerThread(nullptr), m_rootVNode(nullptr) {

    }

    EventManager::~EventManager() {

    }

    bool EventManager::Init() {
        m_workerThread = Thread::CreateKernelThread({&EventManager_WorkThreadMain, this});
        if (m_workerThread == nullptr)
            return false;

        g_EventManager = this;
        return true;
    }

    int EventManager::FullInit() {
        int rc = g_DeviceManager->CreateRootDir("input", DEFAULT_DIR_MODE, &m_rootVNode);
        if (rc < 0)
            return rc;

        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr || s->device == nullptr)
                return true;

            char name[32];
            int len = snprintf(name, 32, "event%lu", static_cast<uint64_t>(id));
            if (len == 0)
                return true; // no name, skip it

            rc = g_DeviceManager->CreateSymlink(name, s->device->GetInfo(), DEFAULT_SYMLINK_MODE, &s->vnode, m_rootVNode);
            return rc == 0;
        });
        m_eventSources.unlock();
        return rc;
    }

    EventSourceID EventManager::RegisterEventSource(EventSourceType type, InputDevice* device) {
        EventSource* s = new EventSource;
        uint64_t id = __atomic_fetch_add(&m_nextEventSourceID, 1, __ATOMIC_ACQ_REL);
        s->id = id;
        s->type = type;
        s->device = device;
        m_eventSources.lock();
        m_eventSources.Insert(id, s);
        m_eventSources.unlock();
        return id;
    }

    InputDevice* EventManager::GetDevice(EventSourceID id, EventSourceType* type) {
        m_eventSources.lock();
        EventSource* s = m_eventSources.Find(id);
        if (s == nullptr) {
            m_eventSources.unlock();
            return nullptr;
        }
        s->lock.Lock();
        m_eventSources.unlock();
        if (type != nullptr)
            *type = s->type;
        InputDevice* d = s->device;
        s->lock.Unlock();
        return d;
    }

    void EventManager::EnumerateEventSources(FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback) {
        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->lock.Lock();
            bool rc = callback(id, s->type, s->device);
            s->lock.Unlock();
            return rc;
        });
        m_eventSources.unlock();
    }

    void EventManager::EnumerateEventSources(uint64_t types, FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback) {
        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->lock.Lock();
            if ((types & static_cast<uint64_t>(s->type)) == 0) {
                s->lock.Unlock();
                return true;
            }
            bool rc = callback(id, s->type, s->device);
            s->lock.Unlock();
            return rc;
        });
        m_eventSources.unlock();
    }

    void EventManager::RaiseEvent(const Event& e) {
        if (m_pendingEvents.push(e))
            m_eventSemaphore.Signal();
    }

    bool EventManager::RegisterEventHandler(EventSourceID id, EventListener& l) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return false;

        s->lock.Lock();
        if (l.m_registered) {
            s->lock.Unlock();
            return false;
        }
        l.m_prev = s->tail;
        l.m_next = nullptr;

        if (s->tail != nullptr)
            s->tail->m_next = &l;
        else
            s->head = &l;

        s->tail = &l;
        l.m_sourceID = id;
        l.m_registered = true;
        s->lock.Unlock();
        return true;
    }

    bool EventManager::UnregisterEventHandler(EventListener& l) {
        if (!l.m_registered)
            return false;

        EventSource* s = FindSource(l.m_sourceID);
        if (s == nullptr)
            return false;
        s->lock.Lock();

        if (l.m_prev != nullptr)
            l.m_prev->m_next = l.m_next;
        else
            s->head = l.m_next;

        if (l.m_next != nullptr)
            l.m_next->m_prev = l.m_prev;
        else
            s->tail = l.m_prev;

        l.m_prev = nullptr;
        l.m_next = nullptr;
        l.m_registered = false;

        s->lock.Unlock();
        return true;
    }

    [[noreturn]] void EventManager::WorkerThreadMain() {
        while (true) {
            m_eventSemaphore.Wait();

            Event e;
            if (!m_pendingEvents.pop(e))
                continue;

            EventSource* s = FindSource(e.source);
            if (s == nullptr)
                continue;

            s->lock.Lock();
            for (EventListener* l = s->head; l != nullptr; l = l->m_next)
                if (l->m_fn(&e, l->m_ctx))
                    break;
            s->lock.Unlock();
        }
    }

    EventManager::EventSource* EventManager::FindSource(EventSourceID id) {
        m_eventSources.lock();
        EventSource* s = m_eventSources.Find(id);
        m_eventSources.unlock();
        return s;
    }

    KeyboardInputSource* g_KBDInputSource = nullptr;

    bool InitTTYEvents() {
        if (g_EventManager == nullptr || g_KTTY == nullptr)
            return false;
        EventSourceID s = UINT64_MAX;
        g_EventManager->EnumerateEventSources(static_cast<uint64_t>(EventSourceType::Keyboard), [&](EventSourceID source, EventSourceType, InputDevice *) -> bool {
            s = source;
            return false;
        });
        if (s == UINT64_MAX)
            return false;
        g_KBDInputSource = new KeyboardInputSource(s);
        return true;
    }

    EventManager* g_EventManager = nullptr;

}