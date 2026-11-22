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

#include "InputManager.hpp"

#include "Backends/InputDevice.hpp"

#include <algorithm>
#include <stdio.h>
#include <time.h>

namespace WM {
    static constexpr uint64_t kBusyRetyMS = 1000;
    static constexpr uint64_t kErrorRetyMS = 5000;

    static uint64_t NowMS() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1'000'000;
    }

    InputManager::InputManager(InputBackend& backend, Seat& seat) : m_backend(backend), m_seat(seat) {

    }

    InputManager::~InputManager() {
        for (Entry& e : m_entries) {
            if (e.claimed)
                e.dev->Release();
        }
    }

    bool InputManager::Initialise() {
        uint64_t now = NowMS();
        for (auto& dev : m_backend.Devices()) {
            m_seat.AddDevice(dev.get());
            m_entries.push_back({dev.get(), false, 0});
            TryClaim(m_entries.back(), now);
        }
        return !m_entries.empty();
    }

    void InputManager::CollectFDs(std::vector<pollfd>& fds) const {
        for (const Entry& e : m_entries) {
            if (e.claimed)
                fds.push_back({e.dev->FD(), POLLIN, 0});
        }
    }

    void InputManager::Dispatch(const pollfd* fds, size_t count) {
        std::vector<InputDevice*> gone;
        std::vector<RawEvent> events;

        size_t i = 0;
        for (Entry& e : m_entries) {
            if (!e.claimed)
                continue;
            if (i >= count)
                break;
            short re = fds[i++].revents;
            if ((re & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) == 0)
                continue;

            events.clear();
            bool alive = e.dev->Read(events);
            m_seat.HandleEvents(*e.dev, events);

            if (!alive)
                gone.push_back(e.dev);
        }

        for (InputDevice* dev : gone) {
            fprintf(stderr, "WindowManager: input device %s went away\n", dev->Path().c_str());
            m_seat.RemoveDevice(dev);
            m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry& e) -> bool {
                return e.dev == dev;
            }), m_entries.end());
        }
    }

    int64_t InputManager::NextTimeoutMS(uint64_t nowMS) const {
        int64_t best = -1;
        for (const Entry& e : m_entries) {
            if (e.claimed)
                continue;
            int64_t wait = e.retryAtMS > nowMS ? (int64_t)(e.retryAtMS - nowMS) : 0;
            if (best < 0 || wait < best)
                best = wait;
        }
        return best;
    }

    void InputManager::Tick(uint64_t nowMS) {
        for (Entry& e : m_entries) {
            if (!e.claimed && nowMS >= e.retryAtMS)
                TryClaim(e, nowMS);
        }
    }

    size_t InputManager::ClaimedCount() const {
        return (size_t)std::count_if(m_entries.begin(), m_entries.end(), [](const Entry& e) {
            return e.claimed;
        });
    }

    void InputManager::TryClaim(Entry& e, uint64_t nowMS) {
        switch (e.dev->Claim()) {
        case ClaimResult::Ok: {
            e.claimed = true;
            KeyBits held;
            if (e.dev->GetKeyState(held))
                m_seat.AdoptKeyState(e.dev, held);
            break;
        }
        case ClaimResult::Busy:
            e.retryAtMS = nowMS + kBusyRetyMS;
            break;
        case ClaimResult::Error:
            e.retryAtMS = nowMS + kErrorRetyMS;
            break;
        }
    }
}