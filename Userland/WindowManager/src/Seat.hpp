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

#ifndef _WM_SEAT_HPP
#define _WM_SEAT_HPP

#include <stdint.h>
#include <unordered_map>

#include "InputConfig.hpp"
#include "Keymap.hpp"
#include "Keys.hpp"
#include "Rect.hpp"

#include "Backends/InputDevice.hpp"

namespace WM {
    struct KeyEvent {
        uint64_t timeNS;
        KeyCode key;
        uint32_t state; // release, press, repeat
        uint32_t codepoint; // text produced by key
    };

    struct ButtonEvent {
        uint64_t timeNS;
        KeyCode button;
        bool pressed;
    };

    struct AxisEvent {
        uint64_t timeNS;
        double dx; // positive = right
        double dy; // positive = down
    };

    class SeatDelegate {
    public:
        virtual ~SeatDelegate() = default;
        virtual void OnKey(const KeyEvent& e) = 0;
        virtual void OnModifiers(uint32_t mods) = 0;
        
        virtual void OnPointerMotion(double x, double y, uint64_t timeNS) = 0;
        virtual void OnPointerButton(const ButtonEvent& e) = 0;
        virtual void OnPointerAxis(const AxisEvent& e) = 0;
    };

    class Seat {
    public:
        Seat(const InputConfig& config, const ActionRegistry& actions, SeatDelegate& delegate);

        void SetPointerBounds(std::vector<Rect> outputs);
        void SetPointerPosition(double x, double y);

        void AddDevice(const InputDevice* dev);
        void RemoveDevice(const InputDevice* dev);
        void AdoptKeyState(const InputDevice* dev, const KeyBits& state);

        void HandleEvents(InputDevice& dev, const std::vector<RawEvent>& events);

        double PointerX() const { return m_x; }
        double PointerY() const { return m_y; }
        uint32_t Modifiers() const { return m_mods; }
        int ButtonsDown() const { return m_buttonsDown; }

    private:
        struct DeviceState {
            KeyBits down;
            bool dropped = false;
        };

        void HandleKey(DeviceState& st, const RawEvent& e);
        void HandleKeyboardKey(const RawEvent& e);
        void HandleMouseButton(const RawEvent& e);
        void Resync(InputDevice& dev, DeviceState& st);

        void FlushPointer();
        double AccelFactor(double velocity) const;
        void ClampPointer();

        void RecomputeMods();
        bool TryBinding(KeyCode key);

        const InputConfig& m_config;
        const ActionRegistry& m_actions;
        SeatDelegate& m_delegate;
        const Keymap* m_keymap;

        std::unordered_map<const InputDevice*, DeviceState> m_devices;
        KeyBits m_swallowed;
        uint32_t m_mods = 0;
        uint32_t m_locks = 0;
        int m_buttonsDown = 0;
        uint64_t m_lastTimeNS = 0;

        std::vector<Rect> m_bounds;
        double m_x = 0, m_y = 0;

        double m_pdx = 0, m_pdy, m_pwheel = 0, m_phwheel = 0;
        uint64_t m_pendingNS = 0, m_lastMotionNS = 0;
    };
}

#endif /* _WM_SEAT_HPP */