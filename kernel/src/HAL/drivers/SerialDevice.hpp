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

#ifndef _HAL_SERIAL_DEVICE_HPP
#define _HAL_SERIAL_DEVICE_HPP

#include <stdint.h>

class SerialDevice {
public:
    SerialDevice() {}
    virtual ~SerialDevice() {}

    virtual int Init() = 0;
    
    virtual bool ReadByte(uint8_t& out, bool block = true) = 0;
    virtual bool WriteByte(uint8_t byte, bool block = true) = 0;

    // Loopback controls. If enabled, the device will send any input to the output
    virtual void EnableLoopback() = 0;
    virtual void DisableLoopback() = 0;
    virtual bool isLoopbackEnabled() const = 0;
};

extern SerialDevice* g_defaultSerialDevice;

#endif /* _HAL_SERIAL_DEVICE_HPP */