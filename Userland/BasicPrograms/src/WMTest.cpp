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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <LibWM/Connection.hpp>
#include <LibWM/Protocol.hpp>
#include <LibWM/SharedMem.hpp>

using namespace LibWM;
namespace P = LibWM::Proto;

static constexpr int kFrames = 240;

static uint32_t Premul(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return a << 24 | ((r * a + 127) / 255) << 16 | ((g * a + 127) / 255) << 8 | ((b * a + 127) / 255);
}

static void FillRect(uint32_t* px, int stridePx, int x0, int y0, int x1, int y1, uint32_t colour) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++)
            px[y * stridePx + x] = colour;
    }
}

static void Draw(uint32_t* px, int w, int h, int stridePx, int frame, uint32_t accent) {
    uint32_t ar = (accent >> 16) & 255, ag = (accent >> 8) & 255, ab = accent & 255;

    FillRect(px, stridePx, 0, 0, w, h, 0);
    FillRect(px, stridePx, 0, 0, w, 24, Premul(255, ar / 3, ag / 3, ab / 3));

    for (int x = 8; x < w - 8; x++) {
        uint32_t a = (uint32_t)(x - 8) * 255 / (uint32_t)(w - 17);
        FillRect(px, stridePx, x, 32, x + 1, 64, Premul(a, ar, ag, ab));
    }

    int steps = 5;
    int bw = (w - 16) / steps;
    for (int i = 0; i < steps; i++) {
        int x0 = 8 + i * bw, x1 = x0 + bw - 4;
        FillRect(px, stridePx, x0, 70, x1, 110, Premul(255, 128, 128, 128));
        FillRect(px, stridePx, x0 + i, 71, x1 - 1, 109, Premul((uint32_t)(i * 255 / (steps - 1)), 255, 255, 255));
    }

    int range = w - 16 - 60;
    int t = (frame * 3) % (2 * range);
    int pos = t < range ? t : 2 * range - t;
    int bx1 = 8 + pos;
    int bx2 = 8 + (range - pos);
    FillRect(px, stridePx, bx1, 118, bx1 + 60, 152, Premul(255, 255, 208, 64));
    FillRect(px, stridePx, bx2, 158, bx2 + 60, 192, Premul(128, 0, 255, 255));

    FillRect(px, stridePx, 0, 0, w, 2, 0xFFFFFFFF);
    FillRect(px, stridePx, 0, h - 2, w, h, 0xFFFFFFFF);
    FillRect(px, stridePx, 0, 0, 2, h, 0xFFFFFFFF);
    FillRect(px, stridePx, w - 2, 0, w, h, 0xFFFFFFFF);
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/run/wm";
    uint32_t accent = argc > 2 ? strtoul(argv[2], nullptr, 0) : 0xFF3030; // 0xRRGGBB

    Connection c = Connection::Connect(path);
    if (!c.Valid()) {
        perror("connect");
        return 1;
    }

    bool welcomed = false, frameDone = false, busy[2] = {false, false};
    auto onEvent = [&](const Message& m) {
        if (m.header.object == 0 && m.header.opcode == P::Display::Welcome)
            welcomed = true;
        else if (m.header.object == 0 && m.header.opcode == P::Display::Error) {
            P::ErrorBody e;
            Parse(m, e);
            fprintf(stderr, "protocol error %u on object %u\n", e.code, e.object);
            exit(1);
        } else if (m.header.object == 2 || m.header.object == 3)
            busy[m.header.object - 2] = false;
        else if (m.header.object == 4)
            frameDone = true;
    };
    auto pump = [&]() {
        if (c.Flush() != Status::Ok) {
            fprintf(stderr, "send failed\n");
            exit(1);
        }
        if (c.Fill() != Status::Ok) {
            fprintf(stderr, "server went away\n");
            exit(1);
        }
        Message m;
        while (c.Next(m))
            onEvent(m);
    };

    c.Send(0, P::Display::Hello, P::Hello{P::Version});
    while (!welcomed)
        pump();

    const uint32_t W = 320, H = 200, stride = W * 4, bufsz = stride * H;
    SharedMemory pool = SharedMemory::Create(bufsz * 2);
    int fd = pool.FD();
    if (fd < 0) {
        perror("shm");
        fflush(stdout);
        return 1;
    }
    c.Send(0, P::Display::CreatePool, P::CreatePool{1, (uint32_t)pool.Size()}, &fd, 1);
    c.Send(0, P::Display::CreateWindow, P::CreateWindow{4});
    c.Send(1, P::Pool::CreateBuffer, P::CreateBuffer{2, 0, W, H, stride, P::ARGB8888});
    c.Send(1, P::Pool::CreateBuffer, P::CreateBuffer{3, bufsz, W, H, stride, P::ARGB8888});

    for (int frame = 0; frame < kFrames; frame++) {
        int i = frame % 2;
        if (busy[i]) {
            fprintf(stderr, "buffer %d still busy!\n", i);
            return 1;
        }
        Draw((uint32_t*)(pool.Data() + i * bufsz), W, H, stride / 4, frame, accent);
        busy[i] = true;
        c.Send(4, P::Window::Attach, P::Attach{(uint32_t)(2 + i)});
        c.Send(4, P::Window::Damage, P::Damage{0, 0, (int)W, (int)H});
        c.Send(4, P::Window::Frame);
        c.Send(4, P::Window::Commit);
        frameDone = false;
        while (!frameDone)
            pump();
    }
    sleep(20);
    puts("client done");
    return 0;
}
