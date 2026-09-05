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

#ifndef _DRIVERS_PS2_CONTROLLER_HPP
#define _DRIVERS_PS2_CONTROLLER_HPP

#include <stddef.h>
#include <stdint.h>

#include "../../Device.hpp"

class PS2Controller : public Device {
public:
    PS2Controller();
    ~PS2Controller();

    int Init() override;
    int Destroy() override;

private:
    int InitialiseDevice(bool channel); // false = 1, true = 2
    int SendCommandToDevice(bool channel, uint8_t command, bool expectAck);
    int ShouldResend();

    int SendCommand(uint8_t command);
    int ReadData(uint8_t* out, size_t attempts = 100000);
    int WriteData(uint8_t data);
    uint8_t GetStatus();
    void Flush();

};

extern PS2Controller* g_PS2Controller;

#endif /* _DRIVERS_PS2_CONTROLLER_HPP */