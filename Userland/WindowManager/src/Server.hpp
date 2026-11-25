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

#ifndef _WM_SERVER_HPP
#define _WM_SERVER_HPP

#include <algorithm>
#include <bitset>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

#include <LibWM/Connection.hpp>
#include <LibWM/Message.hpp>
#include <LibWM/Protocol.hpp>
#include <LibWM/SharedMem.hpp>

#include "Cursor.hpp"
#include "InputConfig.hpp"
#include "InputManager.hpp"
#include "Keys.hpp"
#include "Rect.hpp"
#include "Seat.hpp"

#include "Backends/InputDevice.hpp"
#include "Backends/VideoBackend.hpp"

namespace WM {

    using namespace LibWM;
    namespace P = LibWM::Proto;

    struct BufferObj {
        uint32_t id;
        std::shared_ptr<SharedMemory> pool;
        uint32_t offset, width, height, stride, format;
        bool alive = true;
    };

    struct Insets { int left = 0, top = 0, right = 0, bottom = 0; };

    enum class SizeSource {
        Buffer,  // the client: the frame follows the size of the buffer
        Assigned // the server: the frame has an assigned size
    };

    struct PendingConfigure {
        uint32_t serial;
        int width, height;
        uint32_t flags;
    };

    struct Client;
    struct WindowObj {
        Client* owner;
        uint32_t id;

        Rect frame;
        Insets insets;
        SizeSource sizeSource = SizeSource::Buffer;
        bool placed = false;
        size_t output = 0; // home output, index into Server::outputs

        std::vector<PendingConfigure> configures;
        uint32_t lastConfigureSerial = 0;
        uint32_t ackedSerial = 0;
        uint32_t committedSerial = 0;
        PendingConfigure lastConfigure = {};

        std::shared_ptr<BufferObj> current;
        std::shared_ptr<BufferObj> pending;
        bool pendingAttach = false;
        Rect pendingDamage;
        bool pendingFrame = false, frameCB = false;
        int commits = 0;
        std::bitset<KeyCount> keysSent;

        Rect Content() const {
            return {frame.x0 + insets.left, frame.y0 + insets.top, frame.x1 - insets.right, frame.y1 - insets.bottom};
        }

        Rect GetRect() const {
            if (!current)
                return {frame.x0, frame.y0, frame.x0, frame.y0};
            return frame;
        }

        double LocalX(double gx) const {
            return gx - (frame.x0 + insets.left);
        }

        double LocalY(double gy) const {
            return gy - (frame.y0 + insets.top);
        }
    };

    enum class Type {
        Pool,
        Buffer,
        Window
    };

    struct Obj {
        explicit Obj(Type t) : type(t) {}
        Type type;
        std::shared_ptr<SharedMemory> pool;
        std::shared_ptr<BufferObj> buffer;
        std::shared_ptr<WindowObj> window;
    };

    struct Output {
        VideoDisplay* display = nullptr;
        Rect rect;
        std::vector<uint32_t> back;
        Rect damage;
    };

    struct Client {
        Connection conn;
        std::unordered_map<uint32_t, Obj> objects;
        bool dead = false;
        uint32_t version = 0;

        template <typename T>
        void Send(uint32_t obj, uint16_t op, const T& body) {
            if (!conn.Send(obj, op, body))
                dead = true;
        }

        void Send(uint32_t obj, uint16_t op) {
            if (!conn.Send(obj, op))
                dead = true;
        }

        void Error(uint32_t obj, uint32_t code) {
            conn.Send(0, P::Display::Error, P::ErrorBody{obj, code});
            dead = true;
        }
    };

    class Server : public SeatDelegate {
    public:
        bool Listen(const char* path);

        bool Start(VideoBackend& backend);
        
        void Run();

        void SetExitAfter(size_t n) { clientCount = n; }

        void SetInputConfig(InputConfig config) { inputConfig = std::move(config); }
        bool StartInput(InputBackend& backend);
        void SetTimeout(uint32_t seconds) { timeoutSeconds = seconds; }

        ActionRegistry& Actions() { return actions; }

    private:
        void AcceptClients();
        void Readable(Client& c);
        void Reap();

        // Protocol
        void Dispatch(Client& c, const Message& m);
        void DisplayRequest(Client& c, const Message& m);
        void PoolRequest(Client& c, uint32_t id, Obj& o, const Message& m);
        void WindowRequest(Client& c, uint32_t id, Obj& o, const Message& m);
        void Commit(WindowObj& w);

        void AddDamage(const Rect& r) {
            for (Output& o : outputs)
                o.damage.Add(r.Interset(o.rect));
        }
        bool NeedsFrame() const  {
            return frameScheduled || std::any_of(outputs.begin(), outputs.end(), [](const Output& o) { return !o.damage.Empty(); });
        }
        void Repaint();
        void RepaintOutput(Output& o);
        void DropWindows(Client& c);

        void OnKey(const KeyEvent& e) override;
        void OnModifiers(uint32_t mods) override;
        void OnPointerMotion(double x, double y, uint64_t timeNS) override;
        void OnPointerButton(const ButtonEvent& e) override;
        void OnPointerAxis(const AxisEvent& e) override;

        WindowObj* WindowAt(double x, double y) const;
        void UpdatePointerFocus();
        void SetFocus(WindowObj* w, bool raise);
        void RaiseWindow(WindowObj* w);
        void ForgetWindow(WindowObj* w);
        void FixFocus();
        void DamageCursor();

        void PlaceNewWindow(WindowObj& w);
        void FollowBuffer(WindowObj& w);
        void AssignFrame(WindowObj& w, const Rect& frame, uint32_t flags);
        void ReleaseSize(WindowObj& w, uint32_t flags);
        void SendConfigure(WindowObj& w, int width, int height, uint32_t flags);
        size_t OutputAt(double x, double y) const;

        template <typename T>
        void SendTo(WindowObj* w, uint16_t op, const T& body) {
            w->owner->Send(w->id, op, body);
        }

        Listener listener;
        std::vector<std::unique_ptr<Client>> clients;
        int seen = 0;

        std::vector<std::shared_ptr<WindowObj>> windows;
        std::vector<Output> outputs;
        bool frameScheduled = false;
        int windowSerial = 0;
        uint64_t frameIntervalMS = 16;
        
        size_t clientCount = 0;

        uint32_t timeoutSeconds = 0;

        InputConfig inputConfig = InputConfig::Defaults();
        ActionRegistry actions;
        std::unique_ptr<Seat> seat;
        std::unique_ptr<InputManager> input;

        WindowObj* keyboardFocus = nullptr;
        WindowObj* pointerFocus = nullptr;
        WindowObj* grabWindow = nullptr;
        uint32_t nextSerial = 1;

        std::vector<uint32_t> cursorPixels = BuildCursorPixels();
        Rect cursorRect;
    };

}

#endif /* _WM_SERVER_HPP */