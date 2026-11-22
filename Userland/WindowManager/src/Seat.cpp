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

#include "InputConfig.hpp"
#include "Keys.hpp"
#include "Seat.hpp"

#include "Backends/InputDevice.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <asm/input.h>
#include <asm/key.h>

namespace WM {
    Seat::Seat(const InputConfig& config, const ActionRegistry& actions, SeatDelegate& delegate) : m_config(config), m_actions(actions), m_delegate(delegate) {
        m_keymap = Keymap::Builtin(config.keyboard.layout);
        if (m_keymap == nullptr)
            m_keymap = Keymap::Builtin("us");
        if (config.keyboard.numLockOnStart)
            m_locks |= Mod::NumLock;
        m_mods = m_locks;
    }

    void Seat::SetPointerBounds(std::vector<Rect> outputs) {
        m_bounds = std::move(outputs);
        ClampPointer();
    }

    void Seat::SetPointerPosition(double x, double y) {
        m_x = x;
        m_y = y;
        ClampPointer();
    }

    void Seat::AddDevice(const InputDevice* dev) {
        m_devices.emplace(dev, DeviceState{});
    }

    void Seat::RemoveDevice(const InputDevice* dev) {
        auto it = m_devices.find(dev);
        if (it == m_devices.end())
            return;
        DeviceState& st = it->second;
        for (uint32_t k = 0; k < KeyCount; k++) { // release any pressed keys
            if (st.down.test(k))
                HandleKey(st, RawEvent{m_lastTimeNS, EventType::Key, k, FOSI_KV_RELEASE});
        }
        FlushPointer();
        m_devices.erase(it);
        RecomputeMods();
    }

    void Seat::AdoptKeyState(const InputDevice* dev, const KeyBits& state) {
        auto it = m_devices.find(dev);
        if (it == m_devices.end())
            return;
        uint32_t before = m_mods;
        it->second.down = state;
        m_swallowed |= state;
        RecomputeMods();
        if (m_mods != before)
            m_delegate.OnModifiers(m_mods);
    }

    void Seat::HandleEvents(InputDevice& dev, const std::vector<RawEvent>& events) {
        auto it = m_devices.find(&dev);
        if (it == m_devices.end())
            return;
        DeviceState& st = it->second;

        for (const RawEvent& e : events) {
            m_lastTimeNS = e.timeNS;
            switch (e.type) {
            case EventType::Key:
                if (!st.dropped)
                    HandleKey(st, e);
                break;
            case EventType::Rel:
                if (st.dropped)
                    break;
                m_pendingNS = e.timeNS;
                if (e.code == FOSI_REL_X)
                    m_pdx += (double)e.value;
                else if (e.code == FOSI_REL_Y)
                    m_pdy += (double)e.value;
                else if (e.code == FOSI_REL_WHEEL)
                    m_pwheel += (double)e.value;
                else if (e.code == FOSI_REL_HWHEEL)
                    m_phwheel += (double)e.value;
                break;
            case EventType::Syn:
                if (e.code == FOSI_SYN_DROPPED)
                    st.dropped = true;
                else if (e.code == FOSI_SYN_REPORT && st.dropped) {
                    st.dropped = false;
                    Resync(dev, st);
                }
                break;
            default:
                break; // unknown, ignore
            }
        }
        FlushPointer(); // per batch, not per report
    }

    void Seat::HandleKey(DeviceState& st, const RawEvent& e) {
        KeyCode key = (KeyCode)e.code;
        if (key >= KeyCount)
            return;

        if (e.value == FOSI_KV_PRESS) {
            if (st.down.test(key))
                return;
            st.down.set(key);
        } else if (e.value == FOSI_KV_RELEASE) {
            if (!st.down.test(key))
                return;
            st.down.reset(key);
        } else if (e.value == FOSI_KV_REPEAT) {
            if (!st.down.test(key))
                return;
        } else
            return;
        
        if (IsMouseButton(key))
            HandleMouseButton(e);
        else
            HandleKeyboardKey(e);
    }

    void Seat::HandleKeyboardKey(const RawEvent& e) {
        KeyCode key = (KeyCode)e.code;
        uint32_t state = (uint32_t)e.value;
        uint32_t before = m_mods;

        if (state == FOSI_KV_PRESS) {
            if (key == FOSI_KEY_CAPS_LOCK)
                m_locks ^= Mod::CapsLock;
            if (key == FOSI_KEY_NUM_LOCK)
                m_locks ^= Mod::NumLock;
        }
        RecomputeMods();

        if (state == FOSI_KV_PRESS && ModifierBit(key) == 0 && TryBinding(key)) {
            m_swallowed.set(key);
            return;
        }
        if (m_swallowed.test(key)) {
            if (state == FOSI_KV_RELEASE)
                m_swallowed.reset(key);
            if (m_mods != before)
                m_delegate.OnModifiers(m_mods);
            return;
        }

        uint32_t codepoint = 0;
        if (state != FOSI_KV_RELEASE && ModifierBit(key) == 0 && (m_mods & (Mod::Ctrl | Mod::Alt | Mod::Super)) == 0)
            codepoint = m_keymap->Lookup(key, m_mods, false);

        m_delegate.OnKey({e.timeNS, key, state, codepoint});
        if (m_mods != before)
            m_delegate.OnModifiers(m_mods);
    }

    void Seat::HandleMouseButton(const RawEvent& e) {
        KeyCode button = (KeyCode)e.code;
        if (e.value == FOSI_KV_REPEAT)
            return;
        bool pressed = e.value == FOSI_KV_PRESS;

        if (m_swallowed.test(button)) {
            if (!pressed)
                m_swallowed.reset(button);
            return;
        }

        FlushPointer();

        if (m_config.pointer.leftHanded) {
            if (button == FOSI_KEY_MOUSE_BTN_LEFT)
                button = FOSI_KEY_MOUSE_BTN_RIGHT;
            else if (button == FOSI_KEY_MOUSE_BTN_RIGHT)
                button = FOSI_KEY_MOUSE_BTN_LEFT;
        }

        if (pressed)
            m_buttonsDown++;
        else if (m_buttonsDown > 0)
            m_buttonsDown--;
        m_delegate.OnPointerButton({e.timeNS, button, pressed});
    }

    void Seat::Resync(InputDevice& dev, DeviceState& st) {
        KeyBits actual;
        if (!dev.GetKeyState(actual))
            return;

        for (uint32_t k = 0; k < KeyCount; k++) {
            if (st.down.test(k) && !actual.test(k))
                HandleKey(st, RawEvent{m_lastTimeNS, EventType::Key, k, FOSI_KV_RELEASE});
        }

        for (uint32_t k = 0; k < KeyCount; k++) {
            if (actual.test(k) && ! st.down.test(k)) {
                st.down.set(k);
                m_swallowed.set(k);
            }
        }
        uint32_t before = m_mods;
        RecomputeMods();
        if (m_mods != before)
            m_delegate.OnModifiers(m_mods);
    }

    void Seat::FlushPointer() {
        if (m_pdx != 0 || m_pdy != 0) {
            double dtMS = m_lastMotionNS == 0 ? 50.0 : (double)(m_pendingNS - m_lastMotionNS) / 1e6;
            dtMS = std::clamp(dtMS, 1.0, 50.0);
            double f = AccelFactor(std::hypot(m_pdx, m_pdy) / dtMS);

            m_x += m_pdx * f;
            m_y += m_pdy * f;
            ClampPointer();
            m_lastMotionNS = m_pendingNS;
            m_pdx = 0;
            m_pdy = 0;
            m_delegate.OnPointerMotion(m_x, m_y, m_lastMotionNS);
        }

        if (m_phwheel != 0 || m_phwheel != 0) {
            double sign = m_config.pointer.naturalScroll ? -1.0 : 1.0;
            // kernel wheel is positive = up, protocol dy is positive = down
            AxisEvent a{m_pendingNS, sign * m_phwheel, sign * -m_pwheel};
            m_pwheel = 0;
            m_phwheel = 0;
            m_delegate.OnPointerAxis(a);
        }
    }

    double Seat::AccelFactor(double velocity) const {
        double base = std::pow(2.0, std::clamp(m_config.pointer.speed, -1.0, 1.0));
        if (m_config.pointer.accel == AccelProfile::Flat)
            return base;
        return base * (1.0 + 0.35 * std::min(velocity, 6.0));
    }

    void Seat::ClampPointer() {
        if (m_bounds.empty())
            return;
        double bestX = m_x, bestY = m_y, best = std::numeric_limits<double>::max();
        for (const Rect& r : m_bounds) {
            double cx = std::clamp(m_x, (double)r.x0, (double)r.x1 - 1);
            double cy = std::clamp(m_y, (double)r.y0, (double)r.y1 - 1);
            double d = (cx - m_x) * (cx - m_x) + (cy - m_y) * (cy - m_y);
            if (d < best) {
                best = d;
                bestX = cx;
                bestY = cy;
            }
        }
        m_x = bestX;
        m_y = bestY;
    }

    void Seat::RecomputeMods() {
        static const KeyCode modKeys[] = {FOSI_KEY_LEFT_SHIFT, FOSI_KEY_RIGHT_SHIFT, FOSI_KEY_LEFT_CTRL, FOSI_KEY_RIGHT_CTRL,
                                          FOSI_KEY_LEFT_ALT, FOSI_KEY_RIGHT_ALT, FOSI_KEY_LEFT_SUPER, FOSI_KEY_RIGHT_SUPER};
        uint32_t mods = m_locks;
        for (auto& [dev, st] : m_devices) {
            for (KeyCode k : modKeys) {
                if (st.down.test(k))
                    mods |= ModifierBit(k);
            }
        }
        m_mods = mods;
    }

    bool Seat::TryBinding(KeyCode key) {
        uint32_t mods = m_mods & Mod::Shortcut;
        for (const Binding& b : m_config.bindings) {
            if (b.key == key && b.mods == mods)
                return m_actions.Run(b.action);
        }
        return false;
    }
}