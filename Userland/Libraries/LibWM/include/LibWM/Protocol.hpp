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

#ifndef _FOS_LIBWM_PROTOCOL_HPP
#define _FOS_LIBWM_PROTOCOL_HPP

#include <stdint.h>

namespace LibWM::Proto {
    constexpr uint32_t Version = 2; // Configure is version 2
    constexpr uint32_t LastClientId = 0xFEFFFFFF;

    enum : uint32_t {
        ARGB8888 = 0,
        XRGB8888 = 1
    };

    enum Err : uint32_t {
        InvalidObject = 1,
        InvalidOpcode,
        BadSize,
        IdInUse,
        BadPool,
        BadBuffer,
        BadSerial
    };

    namespace Display {
        enum Request : uint16_t { Hello = 0, Sync = 1, CreatePool = 2, CreateWindow = 3 };
        enum Event   : uint16_t { Welcome = 0, SyncDone = 1, Error = 2 };
    }

    namespace Pool {
        enum Request : uint16_t { CreateBuffer = 0, Destroy = 1 };
    }

    namespace Buffer {
        enum Request : uint16_t { Destroy = 0 };
        enum Event   : uint16_t { Release = 0 };
    }

    namespace Window {
        enum Request : uint16_t { Attach = 0, Damage = 1, Frame = 2, Commit = 3, Destroy = 4, AckConfigure = 5 };
        enum Event   : uint16_t {
            FrameDone = 0,
            PointerEnter = 1, PointerLeave = 2, PointerMotion = 3, PointerButton = 4, PointerAxis = 5,
            KeyboardEntry = 6, KeyboardLeave = 8, Key = 8, Modifiers = 9, Configure = 10
        };
    }

    namespace ConfigureFlag {
        enum : uint32_t { Activated = 1, Maximised = 2, Fullscreen = 4, Tiled = 8 };
    }

    struct Hello        { uint32_t version; };
    struct Welcome      { uint32_t version, width, height; };
    struct Sync         { uint32_t serial; };
    struct SyncDone     { uint32_t serial; };
    struct ErrorBody    { uint32_t object, code; };
    struct CreatePool   { uint32_t newID, size; }; // and an fd
    struct CreateWindow { uint32_t newID; };
    struct CreateBuffer { uint32_t newID, offset, width, height, stride, format; };
    struct Attach       { uint32_t bufferID; };  // 0 = unmap
    struct Damage       { int32_t x, y, w, h; }; // buffer coordinates
    struct FrameDone    { uint32_t timeMS; };
    // Frame, Commit, Destory and Release have no body

    struct PointerEnter  { uint32_t serial; int32_t x, y; };
    struct PointerLeave  { uint32_t serial; };
    struct PointerMotion { uint32_t timeMS; int32_t x, y; };
    struct PointerButton { uint32_t serial, timeMS, button, pressed; };
    struct PointerAxis   { uint32_t timeMS; int32_t dx, dy; };
    struct KeyboardEnter { uint32_t serial; };
    struct KeyboardLeave { uint32_t serial; };
    struct KeyBody       { uint32_t serial, timeMS, key, state, codepoint; }; // state 0 release, 1 press, 2 repeat
    struct ModifiersBody { uint32_t serial, mods; };

    struct Configure    { uint32_t serial, width, height, flags; };
    struct AckConfigure { uint32_t serial; };

    // Coordinates are window local and 24.8 fixed point

    inline int32_t ToFixed(double v) { return (int32_t)(v * 256.0 + (v >= 0 ? 0.5 : -0.5)); }
    inline double FromFixed(int32_t v) { return v / 256.0; }
}

#endif /* _FOS_LIBWM_PROTOCOL_HPP */