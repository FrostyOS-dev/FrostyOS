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

#ifndef _DRIVERS_PS2_KEYBOARD_HPP
#define _DRIVERS_PS2_KEYBOARD_HPP

#include "PS2Controller.hpp"

class PS2Keyboard : public PS2Device {
public:
    PS2Keyboard(PS2Controller* controller, bool channel);
    ~PS2Keyboard() override;

    int Init() override;
    int Init(DeviceManager::DeviceInfo* info) override;

    bool HandleIRQ();

private:
    enum State {
        NORMAL,

        E0,
        PRINT_SCREEN,
        E0_REPEAT,

        E1,
        PAUSE1,
        PAUSE2,
        E1_REPEAT,
        PAUSE3
    } m_state;
};

#endif /* _DRIVERS_PS2_KEYBOARD_HPP */