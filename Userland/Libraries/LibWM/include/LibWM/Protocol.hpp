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
    constexpr uint32_t Version = 1;
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
        BadBuffer
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
        enum Request : uint16_t { Attach = 0, Damage = 1, Frame = 2, Commit = 3, Destroy = 4 };
        enum Event   : uint16_t { FrameDone = 0 };
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
}

#endif /* _FOS_LIBWM_PROTOCOL_HPP */