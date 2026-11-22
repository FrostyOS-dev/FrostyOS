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

#ifndef _WM_INPUT_DEVICE_HPP
#define _WM_INPUT_DEVICE_HPP

#include <bitset>
#include <memory>
#include <stdint.h>
#include <vector>

#include "../Keys.hpp"

namespace WM {
    enum class EventType : uint8_t {
        Key,
        Rel,
        Syn,
        Other
    };

    struct RawEvent {
        uint64_t timeNS; // monotonic
        EventType type;
        uint32_t code;
        int64_t value;
    };

    enum class DeviceType {
        Keyboard,
        Mouse
    };

    typedef std::bitset<KeyCount> KeyBits;
    
    enum class ClaimResult {
        Ok,
        Busy,
        Error
    };

    class InputDevice {
    public:
        virtual ~InputDevice() = default;

        virtual const std::string& Path() const = 0;
        virtual DeviceType Type() const = 0;
        virtual int FD() const = 0;

        virtual ClaimResult Claim() = 0;
        virtual void Release() = 0;

        virtual bool GetKeyState(KeyBits& out) = 0;

        virtual bool Read(std::vector<RawEvent>& out) = 0; // drain, never blocks
    };

    class InputBackend {
    public:
        virtual ~InputBackend() = default;
        virtual bool Initialise() = 0;
        std::vector<std::unique_ptr<InputDevice>>& Devices() { return m_devices; }

    protected:
        std::vector<std::unique_ptr<InputDevice>> m_devices;
    };
}

#endif /* _WM_INPUT_DEVICE_HPP */