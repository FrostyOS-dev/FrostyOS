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

#ifndef _DRIVERS_INPUT_DEVICE_HPP
#define _DRIVERS_INPUT_DEVICE_HPP

#include "../Device.hpp"

enum class InputDeviceType {
    Keyboard,
    Unknown
};

class InputDevice : public Device {
public:
    InputDevice() : m_deviceType(InputDeviceType::Unknown) {}
    InputDevice(InputDeviceType type) : m_deviceType(InputDeviceType::Keyboard) {}
    virtual ~InputDevice() override {};

    virtual int Init() override = 0;
    virtual int Destroy() override = 0;

    inline InputDeviceType GetType() const {
        return m_deviceType;
    }

    inline void SetType(InputDeviceType type) {
        m_deviceType = type;
    }

protected:
    InputDeviceType m_deviceType;
};

#endif /* _DRIVERS_INPUT_DEVICE_HPP */