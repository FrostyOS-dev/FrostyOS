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

#ifndef _DRIVERS_PS2_MOUSE_HPP
#define _DRIVERS_PS2_MOUSE_HPP

#include "PS2Controller.hpp"

class PS2Mouse : public PS2Device {
public:
    enum class Type {
        Standard,
        ScrollWheel,
        FiveButtons
    };

    PS2Mouse(PS2Controller* controller, bool channel);
    ~PS2Mouse() override;

    int Init() override;
    int Init(DeviceManager::DeviceInfo* info) override;

    int SetSampleRate(uint8_t rate);
    int SetResolution(uint8_t res);

    bool HandleIRQ();

private:
    int DetectType();
    void ParsePacket();

    uint8_t m_packet[4];
    uint8_t m_packetBytes;

    Type m_type;
    uint8_t m_lastButtons;
};

#endif /* _DRIVERS_PS2_MOUSE_HPP */