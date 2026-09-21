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

#include "SerialBackend.hpp"

#include "../TTYBackend.hpp"

#include <HAL/drivers/SerialDevice.hpp>

TTYBackendSerial::TTYBackendSerial() : TTYBackend(TTYBackendType::Serial), m_device(nullptr) {

}

TTYBackendSerial::TTYBackendSerial(SerialDevice* dev) : m_device(dev) {

}

char TTYBackendSerial::ReadChar() {
    uint8_t byte = 0;
    m_device->ReadByte(byte);
    return static_cast<char>(byte);
}

bool TTYBackendSerial::ReadCharNoBlock(char* out) {
    uint8_t byte = 0;
    if (!m_device->ReadByte(byte, false))
        return false;
    *out = byte;
    return true;
}

void TTYBackendSerial::WriteChar(char c) {
    m_device->WriteByte(static_cast<uint8_t>(c));
}

void TTYBackendSerial::SetSerialDevice(SerialDevice* dev) {
    m_device = dev;
}
