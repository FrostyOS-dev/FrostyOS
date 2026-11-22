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

#ifndef _WM_INPUT_MANAGER_HPP
#define _WM_INPUT_MANAGER_HPP

#include <poll.h>
#include <stdint.h>
#include <vector>

#include "Seat.hpp"

#include "Backends/InputDevice.hpp"

namespace WM {
    class InputManager {
    public:
        InputManager(InputBackend& backend, Seat& seat);
        ~InputManager();

        bool Initialise();

        void CollectFDs(std::vector<pollfd>& fds) const;
        void Dispatch(const pollfd* fds, size_t count);

        int64_t NextTimeoutMS(uint64_t nowMS) const;
        void Tick(uint64_t nowMS);

        size_t DeviceCount() const { return m_entries.size(); }
        size_t ClaimedCount() const;

    private:
        struct Entry {
            InputDevice* dev;
            bool claimed = false;
            uint64_t retryAtMS = 0;
        };

        void TryClaim(Entry& e, uint64_t nowMS);

        InputBackend& m_backend;
        Seat& m_seat;
        std::vector<Entry> m_entries;
    };
}

#endif /* _WM_INPUT_MANAGER_HPP */