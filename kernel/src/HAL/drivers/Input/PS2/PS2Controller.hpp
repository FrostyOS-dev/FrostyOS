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

#include "../InputDevice.hpp"

#include "../../Device.hpp"

class PS2Controller;

class PS2Device : public InputDevice {
public:
    PS2Device(InputDeviceType type, PS2Controller* controller, bool channel) : InputDevice(type), m_controller(controller), m_channel(channel) {}
    virtual ~PS2Device() {}

    virtual int Init() = 0;
    virtual int Init(DeviceManager::DeviceInfo* info) override = 0;

    bool GetChannel() const { return m_channel; }

protected:
    bool DataAvailable();
    uint8_t ReadData();

    PS2Controller* m_controller;
    bool m_channel;
};

class PS2Controller : public Device {
public:
    PS2Controller();
    ~PS2Controller();

    int Init(DeviceManager::DeviceInfo* info) override;

    static uint8_t GetStatus();
    static int ReadData(uint8_t* out, size_t attempts = 100000);

private:
    int InitialiseDevice(bool channel); // false = 1, true = 2
    int SendCommandToDevice(bool channel, uint8_t command, bool expectAck);
    int ShouldResend();

    int Disable(bool channel);
    int Enable(bool channel);

    static int SendCommand(uint8_t command);
    static int WriteData(uint8_t data);
    static void Flush();

    PS2Device* m_devices[2];
};

extern PS2Controller* g_PS2Controller;

#endif /* _DRIVERS_PS2_CONTROLLER_HPP */