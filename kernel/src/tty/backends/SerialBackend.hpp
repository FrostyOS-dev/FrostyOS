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

#ifndef _TTY_SERIAL_BACKEND_HPP
#define _TTY_SERIAL_BACKEND_HPP

#include "../TTYBackend.hpp"

#include <HAL/drivers/SerialDevice.hpp>

class TTYBackendSerial : public TTYBackend {
public:
    TTYBackendSerial();
    TTYBackendSerial(SerialDevice* dev);

    char ReadChar() override;

    void WriteChar(char c) override;

    void SetSerialDevice(SerialDevice* dev);

private:
    SerialDevice* m_device;
};

#endif /* _TTY_SERIAL_BACKEND_HPP */