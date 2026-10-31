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

#include "SerialTTY.hpp"

#include <string.h>

#include <HAL/drivers/SerialDevice.hpp>

SerialTTY::SerialTTY(SerialDevice* dev) : TTY(TTYType::Serial), m_dev(dev) {

}

SerialTTY::~SerialTTY() {

}

void SerialTTY::SetSerialDevice(SerialDevice* dev) {
    m_dev = dev;
}

SerialDevice* SerialTTY::GetSerialDevice() {
    return m_dev;
}

void SerialTTY::PutChars(const char* s, size_t n) {
    if (m_dev == nullptr)
        return;

    for (size_t i = 0; i < n; i++)
        m_dev->WriteByte(s[i]); // blocking busy write
}

int SerialTTY::GetSize(winsize_t* s) {
    uint64_t v = __atomic_load_n(&m_winSizePacked, __ATOMIC_ACQUIRE);
    memcpy(s, &v, sizeof(*s));
    return 0;
}

int SerialTTY::SetSize(const winsize_t* s) {
    uint64_t v = 0;
    memcpy(&v, s, sizeof(*s));
    __atomic_store_n(&m_winSizePacked, v, __ATOMIC_RELEASE);
    return 0;
}
