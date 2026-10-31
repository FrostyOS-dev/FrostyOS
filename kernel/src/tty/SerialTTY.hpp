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

#ifndef _TTY_SERIAL_HPP
#define _TTY_SERIAL_HPP

#include "TTY.hpp"

class SerialDevice;

class SerialTTY : public TTY {
public:
    SerialTTY(SerialDevice* dev);
    ~SerialTTY() override;

    void SetSerialDevice(SerialDevice* dev);
    SerialDevice* GetSerialDevice();

    const char* GetName() override;

protected:
    void PutChars(const char* s, size_t n) override;
    int GetSize(winsize_t*) override;
    int SetSize(const winsize_t*) override;

private:
    SerialDevice* m_dev;
    uint64_t m_winSizePacked = 0;

    uint64_t m_id;
    char m_name[24];
};

#endif /* _TTY_SERIAL_HPP */