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

    HAL_RegisterLegacyIntHandler(PS2MOUSE_INT, &PS2Mouse_HandleIRQ, this);

    m_eventSourceID = Input::g_EventManager->RegisterEventSource(Input::EventSourceType::Mouse, this);

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

    uint8_t data = ReadData();

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

    Input::Event e{};
    e.type = EventType::MouseEvent;
    e.source = m_eventSourceID;
    e.eventData.mouseEvent.dx = dx;
    e.eventData.mouseEvent.dy = dy;
    bool wheelChanged = false;

    using SD = Input::Event::ScrollDirection;

    if (m_type == Type::ScrollWheel) {
        e.eventData.mouseEvent.scrollDirection = SD::VERTICAL;
        e.eventData.mouseEvent.wheelDelta = static_cast<int8_t>(m_packet[3]);
        if (m_packet[3] != 0)
            wheelChanged = true;
    } else if (m_type == Type::FiveButtons) {
        int16_t wheel = 0;
        wheelChanged = true;
        SD direction = SD::VERTICAL;
        switch (m_packet[3] & 0xF) {
        case 1:
            wheel = 1;
            direction = SD::VERTICAL;
            break;
        case 0xF:
            wheel = -1;
            direction = SD::VERTICAL;
            break;
        case 2:
            wheel = 1;
            direction = SD::HORIZONTAL;
            break;
        case 0xE:
            wheel = -1;
            direction = SD::HORIZONTAL;
            break;
        default:
            wheelChanged = false;
            break;
        }

        buttons |= (m_packet[3] & (1 << 4 | 1 << 5)) >> 1; // bits 4 and 5 are buttons 4 and 5. Input event system expects bits 3 and 4, respectively.
        e.eventData.mouseEvent.scrollDirection = direction;
        e.eventData.mouseEvent.wheelDelta = wheel;
    }

    if (dx != 0 || dy != 0 || wheelChanged)
        g_EventManager->RaiseEvent(e);
    
    if (buttons != m_lastButtons) {
        e.type = EventType::KeyboardKey;
        for (int i = 0; i < 5; i++) {
            if ((buttons & (1 << i)) == (m_lastButtons & (1 << i)))
                continue;
            e.eventData.keyboardEvent.key = static_cast<Key>(static_cast<uint32_t>(Key::MOUSE_BTN_LEFT) + i);
            e.eventData.keyboardEvent.released = (buttons & (1 << i)) == 0;
            g_EventManager->RaiseEvent(e);
        }
        m_lastButtons = buttons;
    }

    m_packetBytes = 0;
}
