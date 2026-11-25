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
#include "Cursor.hpp"
#include "Keys.hpp"

#include <algorithm>
#include <asm/input.h>
#include <cmath>
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
#define LETTERBOX_COLOUR  0xFF0A0A0Au // shown when a window's content is bigger than the buffer from the client

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
        uint64_t deadlineMS = timeoutSeconds > 0 ? NowMS() + (uint64_t)timeoutSeconds * 1000 : 0;

        while (!g_quit && !(clientCount > 0 && (size_t)seen >= clientCount && clients.size() < clientCount)) {
            if (deadlineMS != 0 && NowMS() >= deadlineMS) {
                fprintf(stderr, "WindowManager: timeout reached, exiting\n");
                break;
            }

            std::vector<pollfd> fds;
            fds.push_back({listener.FD(), POLLIN, 0});
            for (auto& c : clients)
                fds.push_back({c->conn.FD(), (short)(POLLIN | (c->conn.WantsWrite() ? POLLOUT : 0)), 0});

            size_t inputStart = fds.size();
            if (input != nullptr)
                input->CollectFDs(fds);
            size_t inputCount = fds.size() - inputStart;

            uint64_t now = NowMS();
            int64_t waitMS = -1;
            auto wakeIn = [&](int64_t ms) {
                if (ms < 0)
                    ms = 0;
                if (waitMS < 0 || ms < waitMS)
                    waitMS = ms;
            };

            if (NeedsFrame())
                wakeIn(nextFrame > now ? (int64_t)(nextFrame - now) : 0);
            if (input != nullptr && input->NextTimeoutMS(now) >= 0)
                wakeIn(input->NextTimeoutMS(now));
            if (deadlineMS != 0)
                wakeIn(deadlineMS > now ? (int64_t)(deadlineMS - now) : 0);

            timespec ts;
            timespec* timeout = nullptr;
            if (waitMS >= 0) {
                ts = {(time_t)(waitMS / 1000), (long)(waitMS % 1000) * 1'000'000};
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

            if (input != nullptr) {
                input->Dispatch(fds.data() + inputStart, inputCount);
                input->Tick(NowMS());
            }

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

    bool Server::StartInput(InputBackend& backend) {
        seat = std::make_unique<Seat>(inputConfig, actions, *this);

        std::vector<Rect> bounds;
        for (const Output& o : outputs)
            bounds.push_back(o.rect);

        seat->SetPointerBounds(bounds);
        seat->SetPointerPosition((outputs[0].rect.x0 + outputs[0].rect.x1) / 2.0, (outputs[0].rect.y0 + outputs[0].rect.y1) / 2.0);

        actions.Register("quit", []() -> void {
            g_quit = 1;
        });
        actions.Register("focus-next", [this]() -> void {
            if (windows.size() > 1)
                SetFocus(windows.front().get(), true);
        });

        DamageCursor();
        input = std::make_unique<InputManager>(backend, *seat);
        return input->Initialise();
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
            c.version = std::clamp(h.version, 1u, P::Version);
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
            w->id = b.newID; // new windows have no position until they first map
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
        case P::Window::AckConfigure: {
            P::AckConfigure a;
            if (!Parse(m, a))
                return c.Error(id, P::BadSize);
            if (a.serial == 0 || a.serial > w.lastConfigureSerial)
                return c.Error(id, P::BadSize);
            auto it = std::find_if(w.configures.begin(), w.configures.end(), [&](const PendingConfigure& p) { return p.serial == a.serial; });
            if (it != w.configures.end()) {
                w.ackedSerial = a.serial;
                w.configures.erase(w.configures.begin(), it + 1);
            }
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
            ForgetWindow(self.get());
            windows.erase(std::remove(windows.begin(), windows.end(), self), windows.end());
            c.objects.erase(id);
            UpdatePointerFocus();
            FixFocus();
            return;
        }
        default:
            return c.Error(id, P::InvalidOpcode);
        }
    }

    void Server::Commit(WindowObj& w) {
        Rect old = w.GetRect();
        bool mapStateChanged = false;

        if (w.pendingAttach) {
            if (w.current && w.current != w.pending && w.current->alive)
                w.owner->Send(w.current->id, P::Buffer::Release);
            mapStateChanged = (w.current == nullptr) != (w.pending == nullptr);
            w.current = w.pending;
            w.pendingAttach = false;

            if (w.current != nullptr) {
                if (!w.placed)
                    PlaceNewWindow(w);
                else if (w.sizeSource == SizeSource::Buffer)
                    FollowBuffer(w);
            }

            Rect nr = w.GetRect();
            if (old != nr) {
                AddDamage(old);
                AddDamage(nr);
            }
        }

        if (w.current) {
            Rect bufr{0, 0, (int)w.current->width, (int)w.current->height};
            Rect content = w.Content();
            AddDamage(w.pendingDamage.Interset(bufr).Moved(content.x0, content.y0).Interset(content));
        }
        w.pendingDamage = {};
        w.committedSerial = w.ackedSerial;

        if (seat != nullptr && w.current != nullptr && keyboardFocus == nullptr)
            SetFocus(&w, false);
        if (mapStateChanged)
            UpdatePointerFocus();

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

            Rect content = w->Content();
            Rect r = w->GetRect().Interset(d);
            if (r.Empty())
                continue;

            const BufferObj& b = *w->current;
            const uint8_t* base = b.pool->Data() + b.offset;
            bool opaque = b.format == P::XRGB8888;

            Rect bufRect{content.x0, content.y0, content.x0 + (int)b.width, content.y0 + (int)b.height};
            Rect covered = bufRect.Interset(r);

            auto letterbox = [&](int y, int xa, int xb) {
                if (xb > xa)
                    std::fill_n(row(y, xa), xb - xa, LETTERBOX_COLOUR);
            };

            for (int y = r.y0; y < r.y1; y++) {
                if (covered.Empty() || y < covered.y0 || y >= covered.y1) {
                    letterbox(y, r.x0, r.x1);
                    continue;
                }
                letterbox(y, r.x0, covered.x0);
                const uint32_t* src = (const uint32_t*)(base + (size_t)(y - content.y0) * b.stride) +  (covered.x0 - content.x0);
                uint32_t* dst = row(y, r.x0);
                for (int x = 0; x < r.x1 - r.x0; x++)
                    dst[x] = opaque ? (src[x] | 0xFF000000) : BlendPreMul(src[x], dst[x]);
                letterbox(y, covered.x1, r.x1);
            }
        }

        if (seat != nullptr) {
            Rect cr = cursorRect.Interset(d);
            for (int y = cr.y0; y < cr.y1; y++) {
                for (int x = cr.x0; x < cr.x1; x++) {
                    uint32_t px = cursorPixels[(size_t)(y - cursorRect.y0) * CursorWidth + (x - cursorRect.x0)];
                    uint32_t* dst = row(y, x);
                    *dst = BlendPreMul(px, *dst);
                }
            }
        }
        
        // Back buffer -> screen
        o.display->Present(o.back.data(), stride, d.Moved(-o.rect.x0, -o.rect.y0));
    }

    void Server::DropWindows(Client& c) {
        for (auto& w : windows) {
            if (w->owner == &c) {
                AddDamage(w->GetRect());
                ForgetWindow(w.get());
            }
        }
        windows.erase(std::remove_if(windows.begin(), windows.end(), [&](auto& w) {
            return w->owner == &c;
        }), windows.end());
        UpdatePointerFocus();
        FixFocus();
    }

    static uint32_t ToMS(uint64_t ns) {
        return (uint32_t)(ns / 1'000'000);
    }

    void Server::OnKey(const KeyEvent& e) {
        WindowObj* w = keyboardFocus;
        if (w == nullptr)
            return;
        // A window only sees releases and repeats for keys whose press it saw
        if (e.state == FOSI_KV_PRESS)
            w->keysSent.reset(e.key);
        else if (!w->keysSent.test(e.key))
            return;
        if (e.state == FOSI_KV_RELEASE)
            w->keysSent.reset(e.key);
        SendTo(w, P::Window::Key, P::KeyBody{nextSerial++, ToMS(e.timeNS), e.key, e.state, e.codepoint});
    }

    void Server::OnModifiers(uint32_t mods) {
        if (keyboardFocus != nullptr)
            SendTo(keyboardFocus, P::Window::Modifiers, P::ModifiersBody{nextSerial++, mods});
    }

    void Server::OnPointerMotion(double x, double y, uint64_t timeNS) {
        DamageCursor();
        UpdatePointerFocus();
        WindowObj* target = grabWindow != nullptr ? grabWindow : pointerFocus;
        if (target != nullptr)
            SendTo(target, P::Window::PointerMotion, P::PointerMotion{ToMS(timeNS), P::ToFixed(target->LocalX(x)), P::ToFixed(target->LocalY(y))});
    }

    void Server::OnPointerButton(const ButtonEvent& e) {
        WindowObj* target = grabWindow != nullptr ? grabWindow : WindowAt(seat->PointerX(), seat->PointerY());

        if (e.pressed) {
            if (seat->ButtonsDown() == 1) // first button
                grabWindow = target;
            if (target != nullptr)
                SetFocus(target, true); // click to focus and raise
        }
        if (target != nullptr)
            SendTo(target, P::Window::PointerButton, P::PointerButton{nextSerial++, ToMS(e.timeNS), e.button, e.pressed ? 1u : 0u});

        if (!e.pressed && seat->ButtonsDown() == 0) {
            grabWindow = nullptr;
            UpdatePointerFocus();
        }
    }

    void Server::OnPointerAxis(const AxisEvent& e) {
        WindowObj* target = grabWindow != nullptr ? grabWindow : pointerFocus;
        if (target != nullptr)
            SendTo(target, P::Window::PointerAxis, P::PointerAxis{ToMS(e.timeNS), P::ToFixed(e.dx), P::ToFixed(e.dy)});
    }

    WindowObj* Server::WindowAt(double x, double y) const {
        int ix = (int)std::floor(x), iy = (int)std::floor(y);
        for (auto it = windows.rbegin(); it != windows.rend(); it++) {
            if ((*it)->current != nullptr && (*it)->GetRect().Contains(ix, iy))
                return it->get();
        }
        return nullptr;
    }

    void Server::UpdatePointerFocus() {
        if (seat == nullptr)
            return;
        WindowObj* target = grabWindow != nullptr ? grabWindow : WindowAt(seat->PointerX(), seat->PointerY());
        if (target == pointerFocus)
            return;
        if (pointerFocus != nullptr)
            SendTo(pointerFocus, P::Window::PointerLeave, P::PointerLeave{nextSerial++});
        pointerFocus = target;
        if (target != nullptr) {
            SendTo(target, P::Window::PointerEnter, P::PointerEnter{nextSerial++, P::ToFixed(target->LocalX(seat->PointerX())), P::ToFixed(target->LocalY(seat->PointerY()))});
        }
    }

    void Server::SetFocus(WindowObj* w, bool raise) {
        if (raise && w != nullptr)
            RaiseWindow(w);
        if (w == keyboardFocus)
            return;

        if (keyboardFocus != nullptr) {
            for (uint32_t k = 0; k < KeyCount; k++) {
                if (keyboardFocus->keysSent.test(k))
                    SendTo(keyboardFocus, P::Window::Key, P::KeyBody{nextSerial++, (uint32_t)NowMS(), k, FOSI_KV_RELEASE, 0});
            }
            keyboardFocus->keysSent.reset();
            SendTo(keyboardFocus, P::Window::KeyboardLeave, P::KeyboardLeave{nextSerial++});
        }
        keyboardFocus = w;
        if (w != nullptr) {
            SendTo(w, P::Window::KeyboardEntry, P::KeyboardEnter{nextSerial++});
            SendTo(w, P::Window::Modifiers, P::ModifiersBody{nextSerial++, seat->Modifiers()});
        }
    }

    void Server::RaiseWindow(WindowObj* w) {
        auto it = std::find_if(windows.begin(), windows.end(), [&](auto& p) {
            return p.get() == w;
        });

        if (it == windows.end() || it + 1 == windows.end())
            return;
        std::rotate(it, it + 1, windows.end());
        AddDamage(w->GetRect());
    }

    void Server::ForgetWindow(WindowObj* w) {
        if (keyboardFocus == w)
            keyboardFocus = nullptr;
        if (pointerFocus == w)
            pointerFocus = nullptr;
        if (grabWindow == w)
            grabWindow = nullptr;
    }

    void Server::FixFocus() {
        if (seat != nullptr && keyboardFocus == nullptr && !windows.empty())
            SetFocus(windows.back().get(), false);
    }

    void Server::DamageCursor() {
        AddDamage(cursorRect);
        int x = (int)std::floor(seat->PointerX()), y = (int)std::floor(seat->PointerY());
        cursorRect = {x, y, x + CursorWidth, y + CursorHeight};
        AddDamage(cursorRect);
    }

    static Insets ResolveInsets(const WindowObj&) {
        return {};
    }

    void Server::PlaceNewWindow(WindowObj& w) {
        double px, py;
        if (seat != nullptr) {
            px = seat->PointerX();
            py = seat->PointerY();
        } else { // no pointer: middle of first output
            px = (outputs[0].rect.x0 + outputs[0].rect.x1) / 2.0;
            py = (outputs[0].rect.y0 + outputs[0].rect.y1) / 2.0;
        }

        w.output = OutputAt(px, py);
        const Rect& out = outputs[w.output].rect;

        w.insets = ResolveInsets(w);
        int fw = (int)w.current->width + w.insets.left + w.insets.right;
        int fh = (int)w.current->height + w.insets.top + w.insets.bottom;

        int x = std::max(out.x0, std::min((int)std::lround(px) - fw / 2, out.x1 - fw));
        int y = std::max(out.y0, std::min((int)std::lround(py) - fh / 2, out.y1 - fh));
        w.frame = {x, y, x + fw, y + fh};
        w.placed = true;
    }

    void Server::FollowBuffer(WindowObj& w) {
        if (!w.current)
            return;
        w.insets = ResolveInsets(w);
        w.frame.x1 = w.frame.x0 + (int)w.current->width + w.insets.left + w.insets.right;
        w.frame.y1 = w.frame.y0 + (int)w.current->height + w.insets.top + w.insets.bottom;
    }

    void Server::AssignFrame(WindowObj& w, const Rect& frame, uint32_t flags) {
        Rect old = w.GetRect();
        w.insets = ResolveInsets(w);
        w.sizeSource = SizeSource::Assigned;
        w.frame = frame;
        w.placed = true;
        AddDamage(old);
        AddDamage(w.GetRect());

        Rect content = w.Content();
        SendConfigure(w, std::max(1, content.Width()), std::max(1, content.Height()), flags);
    }

    void Server::ReleaseSize(WindowObj& w, uint32_t flags) {
        Rect old = w.GetRect();
        w.sizeSource = SizeSource::Buffer;
        FollowBuffer(w);
        AddDamage(old);
        AddDamage(w.GetRect());
        SendConfigure(w, 0, 0, flags);
    }

    void Server::SendConfigure(WindowObj& w, int width, int height, uint32_t flags) {
        if (w.owner->version < 2)
            return;
        if (w.lastConfigureSerial != 0 && w.lastConfigure.width == width && w.lastConfigure.height == height && w.lastConfigure.flags == flags)
            return;

        PendingConfigure p{nextSerial++, width, height, flags};
        w.lastConfigure = p;
        w.lastConfigureSerial = p.serial;
        w.configures.push_back(p);
        if (w.configures.size() > 16)
            w.configures.erase(w.configures.begin());
        SendTo(&w, P::Window::Configure, P::Configure{p.serial, (uint32_t)width, (uint32_t)height, flags});
    }

    size_t Server::OutputAt(double x, double y) const {
        int ix = (int)std::floor(x), iy = (int)std::floor(y);
        for (size_t i = 0; i < outputs.size(); i++) {
            if (outputs[i].rect.Contains(ix, iy))
                return i;
        }
        return 0;
    }
}
