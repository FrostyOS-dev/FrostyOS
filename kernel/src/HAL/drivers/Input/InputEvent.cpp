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
#include "InputDevice.hpp"
#include "InputEvent.hpp"
#include "Key.hpp"

#include "../DeviceManager.hpp"

#include <Function.hpp>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <frostyos/asm/input.h>

#include <fs/VFS.hpp>

#include <HAL/Time.hpp>

#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

#include <tty/Input/KBDInputSource.hpp>

#include <tty/TTY.hpp>

namespace Input {

    [[noreturn]] void EventManager_WorkThreadMain(void* data) {
        static_cast<EventManager*>(data)->WorkerThreadMain();
    }

    static void EventManager_ProcessExit(Process* proc, void* ctx) {
        if (proc != nullptr)
            static_cast<EventManager*>(ctx)->ReleaseClaimsOwnedBy(proc->GetPID());
    }

    int64_t RealTimeOffsetNS() {
        return HAL_RealtimeValid() ? HAL_GetRealtimeOffsetNS() : 0;
    }

    bool FilterFrame(const Frame& in, KeyView& view, Frame* out) {
        out->source = in.source;
        out->monoNS = in.monoNS;
        out->count = 0;

        for (uint32_t i = 0; i < in.count; i++) {
            const Event& e = in.events[i];
            if (e.type == ET_KEY) {
                if (e.code >= KEY_COUNT)
                    continue;
                if (e.value == 1)
                    view.Set(e.code);
                else if (!view.Test(e.code))
                    continue;
                else if (e.value == 0)
                    view.Clear(e.code);
            }
            out->events[out->count++] = e;
        }
        return out->count > 0;
    }

    EventManager::EventManager() : m_nextEventSourceID(0), m_eventSources(), m_pendingEvents(), m_eventSemaphore(0, MAX_INPUT_FRAMES + 1), m_lastDropped(0), m_workerThread(nullptr), m_rootVNode(nullptr) {

    }

    EventManager::~EventManager() {

    }

    bool EventManager::Init() {
        g_EventManager = this;

        if (!Scheduler::RegisterProcessExitHook(&EventManager_ProcessExit, this)) {
            g_EventManager = nullptr;
            return false;
        }

        m_workerThread = Thread::CreateKernelThread({&EventManager_WorkThreadMain, this});
        if (m_workerThread == nullptr) {
            Scheduler::UnregisterProcessExitHook(&EventManager_ProcessExit, this);
            g_EventManager = nullptr;
            return false;
        }
        return true;
    }

    int EventManager::FullInit() {
        int rc = g_DeviceManager->CreateRootDir("input", DEFAULT_DIR_MODE, &m_rootVNode);
        if (rc < 0)
            return rc;

        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->dispatchLock.Lock();
            int r = PublishSourceLocked(s);
            s->dispatchLock.Unlock();
            if (r < 0 && r != -EAGAIN) { // -EAGAIN: not ready yet, driver calls PublishEventSource later
                rc = r;
                return false;
            }
            return true;
        });
        m_eventSources.unlock();
        return rc;
    }

    EventSourceID EventManager::RegisterEventSource(EventSourceType type, InputDevice* device) {
        EventSource* s = new EventSource;
        if (s == nullptr)
            return UINT64_MAX;

        uint64_t id = __atomic_fetch_add(&m_nextEventSourceID, 1, __ATOMIC_ACQ_REL);
        s->id = id;
        s->type = type;
        s->device = device;

        m_eventSources.lock();
        m_eventSources.Insert(id, s);
        m_eventSources.unlock();

        if (device != nullptr)
            device->SetEventSourceID(id);
        return id;
    }

    int EventManager::PublishEventSource(EventSourceID id) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return -ENODEV;
        s->dispatchLock.Lock();
        int rc = PublishSourceLocked(s);
        s->dispatchLock.Unlock();
        return rc;
    }

    void EventManager::UnregisterEventSource(EventSourceID id) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return;

        s->dispatchLock.Lock();
        if (!s->dead) {
            s->dead = true;
            s->device = nullptr;
            s->claimant = nullptr;
            for (InputClient* c = s->clients; c != nullptr; c = c->m_next) {
                c->m_claimed = false;
                c->m_ownerPID = -1;
                c->Hangup();
            }
        }
        s->dispatchLock.Unlock();
    }

    InputDevice* EventManager::GetDevice(EventSourceID id, EventSourceType* type) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return nullptr;
        s->dispatchLock.Lock();
        if (type != nullptr)
            *type = s->type;
        InputDevice* d = s->device;
        s->dispatchLock.Unlock();
        return d;
    }

    void EventManager::EnumerateEventSources(FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback) {
        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->dispatchLock.Lock();
            bool rc = true;
            if (!s->dead)
                rc = callback(id, s->type, s->device);
            s->dispatchLock.Unlock();
            return rc;
        });
        m_eventSources.unlock();
    }

    void EventManager::EnumerateEventSources(uint64_t types, FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback) {
        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID id, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->dispatchLock.Lock();
            bool rc = true;
            if (!s->dead && (types & static_cast<uint64_t>(s->type)) != 0)
                rc = callback(id, s->type, s->device);
            s->dispatchLock.Unlock();
            return rc;
        });
        m_eventSources.unlock();
    }

    void EventManager::RaiseFrame(const Frame& f) {
        m_pendingEvents.push(f);
        m_eventSemaphore.Signal();
    }

    bool EventManager::RegisterEventHandler(EventSourceID id, EventListener& l) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return false;

        s->dispatchLock.Lock();
        if (l.m_registered) {
            s->dispatchLock.Unlock();
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
        s->dispatchLock.Unlock();
        return true;
    }

    bool EventManager::UnregisterEventHandler(EventListener& l) {
        EventSource* s = FindSource(l.m_sourceID);
        if (s == nullptr)
            return false;

        s->dispatchLock.Lock();
        if (!l.m_registered) {
            s->dispatchLock.Unlock();
            return false;
        }

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

        s->dispatchLock.Unlock();
        return true;
    }

    int EventManager::AttachClient(EventSourceID id, InputClient* c) {
        EventSource* s = FindSource(id);
        if (s == nullptr)
            return -ENODEV;

        s->dispatchLock.Lock();
        if (s->dead) {
            s->dispatchLock.Unlock();
            return -ENODEV;
        }
        c->m_source = s;
        c->m_prev = s->clientsTail;
        c->m_next = nullptr;
        if (s->clientsTail != nullptr)
            s->clientsTail->m_next = c;
        else
            s->clients = c;
        s->clientsTail = c;
        s->dispatchLock.Unlock();
        return 0;
    }

    void EventManager::DetachClient(InputClient* c) {
        EventSource* s = c->m_source;
        if (s == nullptr)
            return;

        s->dispatchLock.Lock();
        if (s->claimant == c)
            DropClaimLocked(s);

        if (c->m_prev != nullptr)
            c->m_prev->m_next = c->m_next;
        else
            s->clients = c->m_next;

        if (c->m_next != nullptr)
            c->m_next->m_prev = c->m_prev;
        else
            s->clientsTail = c->m_prev;

        c->m_prev = nullptr;
        c->m_next = nullptr;
        c->m_source = nullptr;
        s->dispatchLock.Unlock();
    }

    int EventManager::SetClaim(InputClient* c, bool claim, Process* proc, const Credential& cred) {
        EventSource* s = c->m_source;
        if (s == nullptr)
            return -ENODEV;
        if (claim && cred.euid != 0)
            return -EPERM;

        s->dispatchLock.Lock();
        int rc = 0;
        if (s->dead)
            rc = -ENODEV;
        else if (claim) {
            if (s->claimant == c)
                rc = 0;
            else if (s->claimant != nullptr)
                rc = -EBUSY;
            else
                TakeClaimLocked(s, c, proc != nullptr ? proc->GetPID() : -1);
        } else {
            if (s->claimant != c)
                rc = -EINVAL;
            else
                DropClaimLocked(s);
        }
        s->dispatchLock.Unlock();
        return rc;
    }

    int EventManager::SetClientClock(InputClient* c, int clock) {
        if (clock != FOSI_CLOCK_REALTIME && clock != FOSI_CLOCK_MONOTONIC)
            return -EINVAL;
        EventSource* s = c->m_source;
        if (s == nullptr)
            return -ENODEV;

        s->dispatchLock.Lock();
        int rc = 0;
        if (s->dead)
            rc = -ENODEV;
        else
            c->SetClock(clock, HAL_GetNSTicks(), RealTimeOffsetNS());
        s->dispatchLock.Unlock();
        return rc;
    }

    int EventManager::GetKeyState(InputClient* c, FOSI_KeyState* out) {
        EventSource* s = c->m_source;
        if (s == nullptr)
            return -ENODEV;

        s->dispatchLock.Lock();
        if (s->dead) {
            s->dispatchLock.Unlock();
            return -ENODEV;
        }

        if (s->claimant != nullptr && s->claimant != c) {
            // someone else owns the device: this client sees nothing
            memset(out->bits, 0, sizeof(out->bits));
        } else {
            memcpy(out->bits, s->deviceDown.bits, sizeof(out->bits));
            c->m_view = s->deviceDown;
        }
        s->dispatchLock.Unlock();
        return 0;
    }

    void EventManager::ReleaseClaimsOwnedBy(int64_t pid) {
        if (pid < 0)
            return;

        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->dispatchLock.Lock();
            if (!s->dead && s->claimant != nullptr && s->claimant->m_ownerPID == pid)
                DropClaimLocked(s);
            s->dispatchLock.Unlock();
            return true;
        });
        m_eventSources.unlock();
    }

    [[noreturn]] void EventManager::WorkerThreadMain() {
        while (true) {
            m_eventSemaphore.Wait();

            Frame f;
            while (m_pendingEvents.pop(f)) {
                EventSource* s = FindSource(f.source);
                if (s == nullptr)
                    continue;

                s->dispatchLock.Lock();
                if (!s->dead)
                    Dispatch(s, f);
                s->dispatchLock.Unlock();
            }

            size_t dropped = m_pendingEvents.dropped();
            if (dropped != m_lastDropped) {
                m_lastDropped = dropped;
                ResyncAllSources();
            }
        }
    }

    EventSource* EventManager::FindSource(EventSourceID id) {
        m_eventSources.lock();
        EventSource* s = m_eventSources.Find(id);
        m_eventSources.unlock();
        return s;
    }

    int EventManager::PublishSourceLocked(EventSource* s) {
        if (m_rootVNode == nullptr || s->dead || s->published || s->device == nullptr)
            return 0;

        DeviceManager::DeviceInfo* info = s->device->GetInfo();
        if (info == nullptr)
            return -EAGAIN;

        char name[32];
        int len = snprintf(name, sizeof(name), "event%lu", static_cast<uint64_t>(s->id));
        if (len <= 0)
            return -EINVAL;

        int rc = g_DeviceManager->CreateSymlink(name, info, DEFAULT_SYMLINK_MODE, &s->vnode, m_rootVNode);
        if (rc == 0)
            s->published = true;
        return rc;
    }

    void EventManager::Dispatch(EventSource* s, const Frame& in) {
        Frame f;
        f.source = in.source;
        f.monoNS = in.monoNS;
        f.count = 0;

        for (uint32_t i = 0; i < in.count; i++) {
            Event e = in.events[i];
            if (e.type == ET_KEY) {
                if (e.code >= KEY_COUNT || e.code == static_cast<uint32_t>(Key::UNKNOWN))
                    continue;
                if (e.value != 0) {
                    e.value = s->deviceDown.Test(e.code) ? 2 : 1;
                    s->deviceDown.Set(e.code);
                } else {
                    if (!s->deviceDown.Test(e.code))
                        continue;
                    s->deviceDown.Clear(e.code);
                }
            } else if (e.type != ET_REL)
                continue;
            f.events[f.count++] = e;
        }

        if (f.count == 0)
            return;

        int64_t off = RealTimeOffsetNS();

        if (s->claimant != nullptr) {
            s->claimant->Deliver(f, off);
            return;
        }

        Frame kf;
        if (FilterFrame(f, s->kernelDown, &kf))
            RunListeners(s, kf);

        for (InputClient* c = s->clients; c != nullptr; c = c->m_next)
            c->Deliver(f, off);
    }

    void EventManager::RunListeners(EventSource* s, const Frame& f) {
        for (EventListener* l = s->head; l != nullptr; l = l->m_next)
            if (l->m_fn(&f, l->m_ctx))
                break;
    }

    void EventManager::ReleaseKernelKeysLocked(EventSource* s, uint64_t monoNS) {
        ForEachKeyReleaseFrame(s->kernelDown, s->id, monoNS, [&](const Frame& f) {
            RunListeners(s, f);
        });
    }

    void EventManager::TakeClaimLocked(EventSource* s, InputClient* c, int64_t ownerPID) {
        uint64_t mono = HAL_GetNSTicks();
        int64_t off = RealTimeOffsetNS();

        s->claimant = c;
        c->m_claimed = true;
        c->m_ownerPID = ownerPID;

        ReleaseKernelKeysLocked(s, mono);
        for (InputClient* o = s->clients; o != nullptr; o = o->m_next)
            if (o != c)
                o->ReleaseAllKeys(mono, off);
    }

    void EventManager::DropClaimLocked(EventSource* s) {
        InputClient* c = s->claimant;
        if (c == nullptr)
            return;
        c->m_claimed = false;
        c->m_ownerPID = -1;
        s->claimant = nullptr;
    }

    void EventManager::ResyncSourceLocked(EventSource* s) {
        uint64_t mono = HAL_GetNSTicks();
        int64_t off = RealTimeOffsetNS();

        s->deviceDown.Reset();
        ReleaseKernelKeysLocked(s, mono);
        for (InputClient* c = s->clients; c != nullptr; c = c->m_next)
            c->Resync(mono, off);
    }

    void EventManager::ResyncAllSources() {
        m_eventSources.lock();
        m_eventSources.forEach([&](EventSourceID, EventSource* s) -> bool {
            if (s == nullptr)
                return true;
            s->dispatchLock.Lock();
            if (!s->dead)
                ResyncSourceLocked(s);
            s->dispatchLock.Unlock();
            return true;
        });
        m_eventSources.unlock();
    }


    FrameBuilder::FrameBuilder(EventSourceID source) : m_frame{}, m_overflow(false) {
        m_frame.source = source;
    }

    void FrameBuilder::AddKey(Key key, bool pressed) {
        if (key == Key::UNKNOWN)
            return;
        if (m_frame.count >= MAX_EVENTS_PER_FRAME) {
            m_overflow = true;
            return;
        }
        m_frame.events[m_frame.count++] = Event{ET_KEY, static_cast<uint32_t>(key), pressed ? 1 : 0};
    }
    
    void FrameBuilder::AddRel(uint32_t code, int32_t value) {
        if (m_frame.count >= MAX_EVENTS_PER_FRAME) {
            m_overflow = true;
            return;
        }
        m_frame.events[m_frame.count++] = Event{ET_REL, code, value};
    }

    bool FrameBuilder::Submit() {
        if (m_overflow || m_frame.count == 0 || g_EventManager == nullptr)
            return false;
        m_frame.monoNS = HAL_GetNSTicks();
        g_EventManager->RaiseFrame(m_frame);
        m_frame.count = 0;
        return true;
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