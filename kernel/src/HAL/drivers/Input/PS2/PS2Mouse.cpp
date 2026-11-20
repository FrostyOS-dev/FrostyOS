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

#include "PS2Controller.hpp"
#include "PS2Mouse.hpp"

#include "../InputDevice.hpp"
#include "../InputEvent.hpp"

#include "../../DeviceManager.hpp"

#include <string.h>

#include <HAL/HAL.hpp>

#ifdef __x86_64__
#define PS2MOUSE_INT 12
#endif

#define PS2MOUSE_SET_SAMPLE_RATE_CMD 0xF3
#define PS2MOUSE_SET_RESOLUTION_CMD 0xE8

uint32_t PS2Mouse_HandleIRQ(void* ctx) {
    PS2Mouse* mouse = static_cast<PS2Mouse*>(ctx);
    if (ctx == nullptr)
        return 0;
    return mouse->HandleIRQ() ? 1 : 0;
}

static uint8_t BytesInPacket(PS2Mouse::Type t) {
    switch (t) {
    case PS2Mouse::Type::Standard:
        return 3;
    case PS2Mouse::Type::ScrollWheel:
    case PS2Mouse::Type::FiveButtons:
        return 4;
    }
    return 3;
}

PS2Mouse::PS2Mouse(PS2Controller* controller, bool channel) : PS2Device(InputDeviceType::Mouse, controller, channel), m_packet(), m_packetBytes(0), m_type(Type::Standard), m_lastButtons(0) {

}

PS2Mouse::~PS2Mouse() {

}

int PS2Mouse::Init() {
    if (Input::g_EventManager == nullptr)
        return -EIO;

    int rc = DetectType();
    if (rc < 0)
        return rc;

    
    m_eventSourceID = Input::g_EventManager->RegisterEventSource(Input::EventSourceType::Mouse, this);
    if (m_eventSourceID == UINT64_MAX)
        return -ENOMEM;

    HAL_RegisterLegacyIntHandler(PS2MOUSE_INT, &PS2Mouse_HandleIRQ, this);
    return 0;
}

int PS2Mouse::Init(DeviceManager::DeviceInfo* info) {
    if (info == nullptr)
        return -EIO;

    m_info = info;

    char name[9];
    memcpy(name, "channel", 7);
    name[7] = m_channel ? '1' : '0';
    name[8] = 0;

    DeviceManager::CreateParams params{};
    params.info = info;
    params.name = name;
    params.type = FS::VType::CHR;
    return g_DeviceManager->CreateRootItem(params);
}

int PS2Mouse::SetSampleRate(uint8_t rate) {
    int rc = SendCommand(PS2MOUSE_SET_SAMPLE_RATE_CMD);
    if (rc < 0)
        return rc;
    return SendCommand(rate);
}

int PS2Mouse::SetResolution(uint8_t res) {
    if (res > 3)
        return -EINVAL;

    int rc = SendCommand(PS2MOUSE_SET_RESOLUTION_CMD);
    if (rc < 0)
        return rc;
    return SendCommand(res);
}

bool PS2Mouse::HandleIRQ() {
    if (!DataAvailable())
        return false;

    uint8_t data;
    if (!ReadData(&data))
        return false;

    if (Input::g_EventManager == nullptr)
        return true; // clear the pending data, but don't do anything with it

    if (m_packetBytes == 0 && (data & (1 << 3)) == 0)
        return true;

    m_packet[m_packetBytes++] = data;
    if (m_packetBytes == BytesInPacket(m_type))
        ParsePacket();

    return true;
}

int PS2Mouse::DetectType() {
#define CHECK(func) rc = func; if (rc < 0) return rc

    int rc = 0;
    auto currentType = [&]() -> Type {
        PS2DeviceID id{};
        rc = m_controller->IdentifyDevice(m_channel, &id);
        if (rc < 0 || id.size != 1) {
            if (rc >= 0)
                rc = -EIO;
            return Type::Standard;
        }
        
        switch (id.id[0]) {
        case 0:
            return Type::Standard;
        case 3:
            return Type::ScrollWheel;
        case 4:
            return Type::FiveButtons;
        }
        rc = -EIO;
        return Type::Standard;
    };

    Type t = currentType();
    if (rc < 0)
        return rc;
    m_type = t;
    if (m_type == Type::Standard) {
        CHECK(SetSampleRate(200));
        CHECK(SetSampleRate(100));
        CHECK(SetSampleRate(80));

        m_type = currentType();
        if (rc < 0)
            return rc;
    }

    if (m_type == Type::ScrollWheel) {
        CHECK(SetSampleRate(200));
        CHECK(SetSampleRate(200));
        CHECK(SetSampleRate(80));

        m_type = currentType();
        if (rc < 0)
            return rc;
    }

    return 0;
#undef CHECK
}

void PS2Mouse::ParsePacket() {
    using namespace Input;

    uint8_t buttons = m_packet[0] & 0x7; // bits 0-2 are left, right, middle
    int32_t dx = 0, dy = 0;
    bool xSign = ((m_packet[0] & (1 << 4)) > 0), ySign = ((m_packet[0] & (1 << 5)) > 0); // bit 4 is x sign, 5 is y sign
    dx = static_cast<int32_t>(static_cast<uint32_t>(m_packet[1]) | (xSign ? 0xFFFFFF00 : 0));
    dy = static_cast<int32_t>(static_cast<uint32_t>(m_packet[2]) | (ySign ? 0xFFFFFF00 : 0));

    // PS/2: Y positive = up, wheel positive = towards the user, negate both
    int32_t wheel = 0;
    uint32_t wheelAxis = REL_WHEEL;

    if (m_type == Type::ScrollWheel)
        wheel = -static_cast<int32_t>(static_cast<int8_t>(m_packet[3]));
    else if (m_type == Type::FiveButtons) {
        switch (m_packet[3] & 0xF) {
        case 0x1: wheel = -1; break;
        case 0xF: wheel =  1; break;
        case 0x2: wheel =  1; wheelAxis = REL_HWHEEL; break;
        case 0xE: wheel = -1; wheelAxis = REL_HWHEEL; break;
        default: break;
        }
        buttons |= (m_packet[3] & (1 << 4 | 1 << 5)) >> 1; // buttons 4 and 5 move to bits 3 and 4
    }

    FrameBuilder f(m_eventSourceID);
    if (dx != 0)
        f.AddRel(REL_X, dx);
    if (dy != 0)
        f.AddRel(REL_Y, -dy);
    if (wheel != 0)
        f.AddRel(wheelAxis, wheel);

    uint8_t changed = buttons ^ m_lastButtons;
    for (int i = 0; i < 5; i++) {
        if ((changed & (1 << i)) == 0)
            continue;
        f.AddKey(static_cast<Key>(static_cast<uint32_t>(Key::MOUSE_BTN_LEFT) + i), (buttons & (1 << i)) != 0);
    }
    f.Submit();

    m_lastButtons = buttons;
    m_packetBytes = 0;
}
