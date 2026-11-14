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

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <unordered_map>

#include <LibWM/Connection.hpp>
#include <LibWM/Message.hpp>
#include <LibWM/Protocol.hpp>
#include <LibWM/SharedMem.hpp>

#include "Rect.hpp"
#include "VideoBackend.hpp"

namespace WM {

    using namespace LibWM;
    namespace P = LibWM::Proto;

    struct BufferObj {
        uint32_t id;
        std::shared_ptr<SharedMemory> pool;
        uint32_t offset, width, height, stride, format;
        bool alive = true;
    };

    struct Client;
    struct WindowObj {
        Client* owner;
        uint32_t id;
        int x = 0, y = 0;
        std::shared_ptr<BufferObj> current;
        std::shared_ptr<BufferObj> pending;
        bool pendingAttach = false;
        Rect pendingDamage;
        bool pendingFrame = false, frameCB = false;
        int commits = 0;
        Rect GetRect() const {
            if (!current)
                return {x, y, x, y};
            return {x, y, x + (int)current->width, y + (int)current->height};
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

    class Server {
    public:
        bool Listen(const char* path);

        bool Start(VideoBackend& backend);
        
        void Run();

        void SetExitAfter(size_t n) { clientCount = n; }

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

        Listener listener;
        std::vector<std::unique_ptr<Client>> clients;
        int seen = 0;

        std::vector<std::shared_ptr<WindowObj>> windows;
        std::vector<Output> outputs;
        bool frameScheduled = false;
        int windowSerial = 0;
        uint64_t frameIntervalMS = 16;
        
        size_t clientCount = 0;
    };

}

#endif /* _WM_SERVER_HPP */