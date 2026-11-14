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

#include "Server.hpp"

#include <algorithm>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <LibWM/Protocol.hpp>
#include <LibWM/SharedMem.hpp>

#include <sys/poll.h>
#include <sys/signal.h>

#define BACKGROUND_COLOUR 0xFF203040u

namespace WM {
    static volatile sig_atomic_t g_quit = 0;

    static void OnSignal(int) {
        g_quit = 1;
    }

    static uint64_t NowMS() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }

    static bool IDOk(const Client& c, uint32_t id) {
        return id >= 1 && id <= P::LastClientId && !c.objects.count(id);
    }

    bool Server::Listen(const char* path) {
        return listener.Listen(path);
    }

    bool Server::Start(VideoBackend& backend) {
        int x = 0;
        uint32_t maxRefresh = 0;
        for (size_t i = 0; i < backend.DisplayCount(); i++) {
            VideoDisplay& d = backend.GetDisplay(i);
            const VideoMode& m = d.CurrentMode();

            Output o;
            o.display = &d;
            o.rect = {x, 0, x + (int)m.width, (int)m.height};
            o.back.assign((size_t)m.width * m.height, BACKGROUND_COLOUR);
            o.damage = o.rect;
            outputs.push_back(std::move(o));

            x += (int)m.width;
            maxRefresh = std::max(maxRefresh, m.refreshRate);
        }
        if (outputs.empty())
            return false;

        uint32_t hz = maxRefresh > 0 ? maxRefresh : 60;
        frameIntervalMS = (1000 + hz / 2) / hz;

        return true;
    }

    void Server::Run() {
        sigset_t block, orig;
        sigemptyset(&block);
        sigaddset(&block, SIGINT);
        sigaddset(&block, SIGTERM);
        sigprocmask(SIG_BLOCK, &block, &orig);
        struct sigaction sa = {};
        sa.sa_handler = OnSignal;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        uint64_t nextFrame = 0;

        while (!g_quit && !(clientCount > 0 && (size_t)seen >= clientCount && clients.size() < clientCount)) {
            std::vector<pollfd> fds;
            fds.push_back({listener.FD(), POLLIN, 0});
            for (auto& c : clients)
                fds.push_back({c->conn.FD(), (short)(POLLIN | (c->conn.WantsWrite() ? POLLOUT : 0)), 0});

            timespec ts;
            timespec* timeout = nullptr;
            if (NeedsFrame()) {
                uint64_t now = NowMS(), wait = nextFrame > now ? nextFrame - now : 0;
                ts = {(time_t)(wait / 1000), (long)(wait % 1000) * 1000000};
                timeout = &ts;
            }

            int n = ppoll(fds.data(), fds.size(), timeout, &orig);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                perror("ppoll");
                break;
            }

            size_t existing = clients.size();
            for (size_t i = 0; i < existing; i++) {
                Client& c = *clients[i];
                short re = fds[i + 1].revents;
                if ((re & POLLOUT) > 0) {
                    Status s = c.conn.Flush();
                    if (s == Status::Closed || s == Status::Error)
                        c.dead = true;
                }
                if ((re & (POLLIN | POLLHUP | POLLERR)) > 0)
                    Readable(c);
            }
            if ((fds[0].revents & POLLIN) > 0)
                AcceptClients();

            if (NeedsFrame() && NowMS() >= nextFrame) {
                Repaint();
                nextFrame = NowMS() + frameIntervalMS;
            }

            for (auto& c : clients) {
                if (c->dead || !c->conn.WantsWrite())
                    continue;
                Status s = c->conn.Flush();
                if (s == Status::Closed || s == Status::Error)
                    c->dead = true;
            }
            Reap();
        }
    }

    void Server::AcceptClients() {
        while (true) {
            Connection conn = listener.Accept();
            if (!conn.Valid())
                break;
            auto c = std::make_unique<Client>();
            c->conn = std::move(conn);
            clients.push_back(std::move(c));
            seen++;
        }
    }

    void Server::Readable(Client& c) {
        Status s = c.conn.Fill();
        Message m;
        while (!c.dead && c.conn.Next(m))
            Dispatch(c, m);
        if (c.conn.ProtocolError() || s == Status::Protocol || s == Status::Closed || s == Status::Error)
            c.dead = true;
    }

    void Server::Reap() {
        for (auto& c : clients) {
            if (c->dead) {
                c->conn.Flush();
                DropWindows(*c);
            }
        }
        clients.erase(std::remove_if(clients.begin(), clients.end(), [](auto& c) {
            return c->dead;
        }), clients.end());
    }

    void Server::Dispatch(Client& c, const Message& m) {
        uint32_t id = m.header.object;
        if (id == 0)
            return DisplayRequest(c, m);
        auto it = c.objects.find(id);
        if (it == c.objects.end())
            return c.Error(id, P::InvalidObject);

        Obj& o = it->second;
        switch (o.type) {
        case Type::Pool:
            return PoolRequest(c, id, o, m);
        case Type::Window:
            return WindowRequest(c, id, o, m);
        case Type::Buffer:
            if (m.header.opcode != P::Buffer::Destroy)
                return c.Error(id, P::InvalidOpcode);
            o.buffer->alive = false;
            c.objects.erase(it);
            return;
        }
    }

    void Server::DisplayRequest(Client& c, const Message& m) {
        switch (m.header.opcode) {
        case P::Display::Hello: {
            P::Hello h;
            if (!Parse(m, h))
                return c.Error(0, P::BadSize);
            c.Send(0, P::Display::Welcome, P::Welcome({P::Version, (uint32_t)outputs[0].rect.Width(), (uint32_t)outputs[0].rect.Height()}));
            return;
        }
        case P::Display::Sync: {
            P::Sync s;
            if (!Parse(m, s))
                return c.Error(0, P::BadSize);
            c.Send(0, P::Display::SyncDone, P::SyncDone{s.serial});
            return;
        }
        case P::Display::CreatePool: {
            int fd = c.conn.TakeFD(); // always consume the fd
            P::CreatePool b;
            if (!Parse(m, b)) {
                if (fd >= 0)
                    close(fd);
                return c.Error(0, P::BadSize);
            }
            if (!IDOk(c, b.newID)) {
                if (fd >= 0)
                    close(fd);
                return c.Error(0, P::IdInUse);
            }
            SharedMemory shm = SharedMemory::Import(fd, b.size); // closes fd on failure
            if (!shm.Valid())
                return c.Error(0, P::BadPool);
            Obj o(Type::Pool);
            o.pool = std::make_shared<SharedMemory>(std::move(shm));
            c.objects.emplace(b.newID, std::move(o));
            return;
        }
        case P::Display::CreateWindow: {
            P::CreateWindow b;
            if (!Parse(m, b))
                return c.Error(0, P::BadSize);
            if (!IDOk(c, b.newID))
                return c.Error(0, P::IdInUse);
            auto w = std::make_shared<WindowObj>();
            w->owner = &c;
            w->id = b.newID;
            w->x = 30 + 70 * windowSerial; // cascade
            w->y = 30 + 50 * windowSerial;
            windowSerial++;
            Obj o(Type::Window);
            o.window = w;
            c.objects.emplace(b.newID, std::move(o));
            windows.push_back(w);
            return;
        }
        default:
            return c.Error(0, P::InvalidOpcode);
        }
    }

    void Server::PoolRequest(Client& c, uint32_t id, Obj& o, const Message& m) {
        switch (m.header.opcode) {
        case P::Pool::CreateBuffer: {
            P::CreateBuffer b;
            if (!Parse(m, b))
                return c.Error(id, P::BadSize);
            if (!IDOk(c, b.newID))
                return c.Error(id, P::IdInUse);
            bool ok = (b.format == P::ARGB8888 || b.format == P::XRGB8888) &&
                      b.width > 0 && b.height > 0 && b.width <= 8192 && b.height <= 8192 &&
                      b.stride >= b.width * 4 && b.stride % 4 == 0 && b.offset % 4 == 0 &&
                      BufferInBounds(o.pool->Size(), b.offset, b.height, b.stride);
            if (!ok)
                return c.Error(id, P::BadBuffer);
            auto buf = std::make_shared<BufferObj>(BufferObj{b.newID, o.pool, b.offset, b.width, b.height, b.stride, b.format});
            Obj n(Type::Buffer);
            n.buffer = buf;
            c.objects.emplace(b.newID, std::move(n));
            return;
        }
        case P::Pool::Destroy:
            c.objects.erase(id);
            return;
        default:
            return c.Error(id, P::InvalidOpcode);
        }
    }

    void Server::WindowRequest(Client& c, uint32_t id, Obj& o, const Message& m) {
        WindowObj& w = *o.window;
        switch (m.header.opcode) {
        case P::Window::Attach: {
            P::Attach a;
            if (!Parse(m, a))
                return c.Error(id, P::BadSize);
            if (a.bufferID == 0)
                w.pending = nullptr;
            else {
                auto it = c.objects.find(a.bufferID);
                if (it == c.objects.end() || it->second.type != Type::Buffer)
                    return c.Error(id, P::BadBuffer);
                w.pending = it->second.buffer;
            }
            w.pendingAttach = true;
            return;
        }
        case P::Window::Damage: {
            P::Damage d;
            if (!Parse(m, d))
                return c.Error(id, P::BadSize);
            if (d.w > 0 && d.h > 0)
                w.pendingDamage.Add({d.x, d.y, d.x + d.w, d.y + d.h});
            return;
        }
        case P::Window::Frame:
            w.pendingFrame = true;
            return;
        case P::Window::Commit:
            Commit(w);
            return;
        case P::Window::Destroy: {
            AddDamage(w.GetRect());
            auto self = o.window;
            windows.erase(std::remove(windows.begin(), windows.end(), self), windows.end());
            c.objects.erase(id);
            return;
        }
        default:
            return c.Error(id, P::InvalidOpcode);
        }
    }

    void Server::Commit(WindowObj& w) {
        Rect old = w.GetRect();
        if (w.pendingAttach) {
            if (w.current && w.current != w.pending && w.current->alive)
                w.owner->Send(w.current->id, P::Buffer::Release);
            w.current = w.pending;
            w.pendingAttach = false;
            Rect nr = w.GetRect();
            if (old.x1 - old.x0 != nr.x1 - nr.x0 || old.y1 - old.y0 != nr.y1 - nr.y0) {
                AddDamage(old);
                AddDamage(nr);
            }
        }
        if (w.current) {
            Rect bufr{0, 0, (int)w.current->width, (int)w.current->height};
            AddDamage(w.pendingDamage.Interset(bufr).Moved(w.x, w.y));
        }
        w.pendingDamage = {};
        if (w.pendingFrame) {
            w.frameCB = true;
            w.pendingFrame = false;
            frameScheduled = true;
        }
        w.commits++;
    }

    static inline uint32_t BlendPreMul(uint32_t s, uint32_t d) {
        uint32_t a = s >> 24;
        if (a == 255)
            return s;
        if (a == 0)
            return d;
        uint32_t ia = 255 - a;
        auto ch = [&](int sh) {
            uint32_t v = ((s >> sh) & 255) + (((d >> sh) & 255) * ia + 127) / 255;
            return v > 255 ? 255u : v;
        };
        return 0xFF000000 | ch(16) << 16 | ch(8) << 8 | ch(0);
    }

    void Server::Repaint() {
        frameScheduled = false;
        for (Output& o : outputs)
            RepaintOutput(o);

        uint32_t t = (uint32_t)NowMS();
        for (auto& w : windows) {
            if (w->frameCB) {
                w->frameCB = false;
                w->owner->Send(w->id, P::Window::FrameDone, P::FrameDone{t});
            }
        }
    }

    void Server::RepaintOutput(Output& o) {
        Rect d = o.damage;
        o.damage = {};
        frameScheduled = false;
        if (d.Empty())
            return;

        const size_t stride = (size_t)o.rect.Width();
        auto row = [&](int y, int x) {
            return &o.back[(size_t)(y - o.rect.y0) * stride + (x - o.rect.x0)];
        };

        for (int y = d.y0; y < d.y1; y++)
            std::fill_n(row(y, d.x0), d.x1 - d.x0, BACKGROUND_COLOUR);

        for (auto& w : windows) {
            if (!w->current)
                continue;
            Rect r = w->GetRect().Interset(d);
            if (r.Empty())
                continue;
            const BufferObj& b = *w->current;
            const uint8_t* base = b.pool->Data() + b.offset;
            bool opaque = b.format == P::XRGB8888;
            for (int y = r.y0; y < r.y1; y++) {
                const uint32_t* src = (const uint32_t*)(base + (size_t)(y - w->y) * b.stride) +  (r.x0 - w->x);
                uint32_t* dst = row(y, r.x0);
                for (int x = 0; x < r.x1 - r.x0; x++)
                    dst[x] = opaque ? (src[x] | 0xFF000000) : BlendPreMul(src[x], dst[x]);
            }
        }
        
        // Back buffer -> screen
        o.display->Present(o.back.data(), stride, d.Moved(-o.rect.x0, -o.rect.y0));
    }

    void Server::DropWindows(Client& c) {
        for (auto& w : windows) {
            if (w->owner == &c)
                AddDamage(w->GetRect());
        }
        windows.erase(std::remove_if(windows.begin(), windows.end(), [&](auto& w) {
            return w->owner == &c;
        }), windows.end());
    }

}