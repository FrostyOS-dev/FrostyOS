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
#include <stddef.h>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/Buffer.hpp>

#include <fs/DevTempFS/DevTempFS.hpp>

#include <frostyos/asm/input.h>

#include <Scheduling/Mutex.hpp>
#include <Scheduling/Semaphore.hpp>

#include "Key.hpp"

#define MAX_INPUT_FRAMES 256

class InputDevice;
class InputClient;
class Process;
class Thread;
struct Credential;

namespace Input {

    typedef uint64_t EventSourceID;

    enum class EventSourceType : uint64_t {
        Keyboard = 1 << 0,
        Mouse    = 1 << 1
    };

    constexpr uint32_t ET_KEY = FOSI_ET_KEY;
    constexpr uint32_t ET_REL = FOSI_ET_REL;

    constexpr uint32_t REL_X = FOSI_REL_X;
    constexpr uint32_t REL_Y = FOSI_REL_Y;
    constexpr uint32_t REL_WHEEL = FOSI_REL_WHEEL;
    constexpr uint32_t REL_HWHEEL = FOSI_REL_HWHEEL;

    constexpr size_t MAX_EVENTS_PER_FRAME = 8;

    struct Event {
        uint32_t type;
        uint32_t code;
        int32_t value;
    };

    struct Frame {
        EventSourceID source;
        uint64_t monoNS;
        uint32_t count;
        Event events[MAX_EVENTS_PER_FRAME];
    };

    struct KeyView {
        uint8_t bits[FOSI_KEY_BITMAP_BYTES];

        KeyView() { Reset(); }
        void Reset() { for (size_t i = 0; i < sizeof(bits); i++) bits[i] = 0; }
        bool Test(uint32_t k) const { return (bits[k >> 3] >> (k & 7)) & 1; }
        void Set(uint32_t k) { bits[k >> 3] |= static_cast<uint8_t>(1u << (k & 7)); }
        void Clear(uint32_t k) { bits[k >> 3] &= static_cast<uint8_t>(~(1u << (k & 7))); }
    };

    bool FilterFrame(const Frame& in, KeyView& view, Frame* out);

    template <typename Fn>
    inline void ForEachKeyReleaseFrame(KeyView& view, EventSourceID source, uint64_t monoNS, Fn&& emit) {
        Frame f{};
        f.source = source;
        f.monoNS = monoNS;
        f.count = 0;
        for (uint32_t k = 0; k < KEY_COUNT; k++) {
            if (!view.Test(k))
                continue;
            view.Clear(k);
            f.events[f.count++] = Event{ET_KEY, k, 0};
            if (f.count == MAX_EVENTS_PER_FRAME) {
                emit(static_cast<const Frame&>(f));
                f.count = 0;
            }
        }
        if (f.count > 0)
            emit(static_cast<const Frame&>(f));
    }

    // Kernel listeners
    // true = consumed, stops later kernel listeners, userspace readers still get the frame
    // May sleep, but must NOT call back into the EventManager
    typedef bool (*FrameHandlerFunc)(const Frame* f, void* ctx);

    class EventListener {
    public:
        EventListener(FrameHandlerFunc fn, void* ctx) : m_fn(fn), m_ctx(ctx) {}
        EventListener(const EventListener&) = delete;
        EventListener& operator=(const EventListener&) = delete;
    private:
        friend class EventManager;

        FrameHandlerFunc m_fn;
        void* m_ctx;
        EventListener* m_prev = nullptr;
        EventListener* m_next = nullptr;
        EventSourceID m_sourceID = 0;
        bool m_registered = false;
    };

    struct EventSource {
        EventSourceID id;
        EventSourceType type;
        InputDevice* device;
        FS::DevTempFSVNode* vnode = nullptr;
        bool published = false;

        Mutex dispatchLock;
        bool dead = false;
        EventListener* head = nullptr;
        EventListener* tail = nullptr;
        InputClient* clients = nullptr;
        InputClient* clientsTail = nullptr;
        InputClient* claimant = nullptr;
        KeyView deviceDown;
        KeyView kernelDown;
    };

    class EventManager {
    public:
        EventManager();
        ~EventManager();

        bool Init();
        int FullInit(); // initialise nodes in the devfs

        EventSourceID RegisterEventSource(EventSourceType type, InputDevice* device);
        int PublishEventSource(EventSourceID id);
        void UnregisterEventSource(EventSourceID id);

        InputDevice* GetDevice(EventSourceID id, EventSourceType* type = nullptr);

        // For the following 2 functions:
        // The list of event sources and the event source itself are locked during callbacks, so no EventManager operations can be called.
        void EnumerateEventSources(FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback);
        void EnumerateEventSources(uint64_t types, FunctionRef<bool(EventSourceID id, EventSourceType type, InputDevice* device)> callback);

        void RaiseFrame(const Frame& f);

        bool RegisterEventHandler(EventSourceID id, EventListener& l);
        bool UnregisterEventHandler(EventListener& l);

        int AttachClient(EventSourceID id, InputClient* c);
        void DetachClient(InputClient* c);
        int SetClaim(InputClient* c, bool claim, Process* proc, const Credential& cred);
        int SetClientClock(InputClient* c, int clock);
        int GetKeyState(InputClient* c, FOSI_KeyState* out);

        void ReleaseClaimsOwnedBy(int64_t pid);

        [[noreturn]] void WorkerThreadMain();

    private:
        EventSource* FindSource(EventSourceID id);
        int PublishSourceLocked(EventSource* s);

        void Dispatch(EventSource* s, const Frame& f);
        void RunListeners(EventSource* s, const Frame& f);
        void ReleaseKernelKeysLocked(EventSource* s, uint64_t monoNS);
        void TakeClaimLocked(EventSource* s, InputClient* c, int64_t ownerPID);
        void DropClaimLocked(EventSource* s);
        void ResyncSourceLocked(EventSource* s);

        void ResyncAllSources();

        uint64_t m_nextEventSourceID;
        AVLTree::wAVLTree<EventSourceID, EventSource*> m_eventSources;

        MPSCRingBuffer<Frame, MAX_INPUT_FRAMES> m_pendingEvents;
        Semaphore m_eventSemaphore;
        size_t m_lastDropped;

        Thread* m_workerThread;

        FS::DevTempFSVNode* m_rootVNode;
    };

    class FrameBuilder {
    public:
        explicit FrameBuilder(EventSourceID source);

        void AddKey(Key key, bool pressed); // Key::UNKNOWN is ignored
        void AddRel(uint32_t code, int32_t value);

        bool Submit();

    private:
        Frame m_frame;
        bool m_overflow;
    };

    int64_t RealTimeOffsetNS();

    bool InitTTYEvents();

    extern EventManager* g_EventManager;
}

#endif /* _HAL_INPUT_EVENT_HPP */