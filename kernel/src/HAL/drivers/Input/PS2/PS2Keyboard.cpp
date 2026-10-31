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
#include "PS2Keyboard.hpp"

#include "../InputDevice.hpp"
#include "../InputEvent.hpp"

#include "../../DeviceManager.hpp"

#include <string.h>

#include <HAL/HAL.hpp>

#ifdef __x86_64__
#define PS2KEYBOARD_INT 1
#endif

static constexpr Key g_singleByteKeys[0x59] = {
    Key::UNKNOWN,
    Key::ESC,
    Key::ONE,
    Key::TWO,
    Key::THREE,
    Key::FOUR,
    Key::FIVE,
    Key::SIX,
    Key::SEVEN,
    Key::EIGHT,
    Key::NINE,
    Key::ZERO,
    Key::HYPHEN,
    Key::EQUAL,
    Key::BACKSPACE,
    Key::TAB,
    Key::Q,
    Key::W,
    Key::E,
    Key::R,
    Key::T,
    Key::Y,
    Key::U,
    Key::I,
    Key::O,
    Key::P,
    Key::OPEN_BRACKET,
    Key::CLOSE_BRACKET,
    Key::ENTER,
    Key::LEFT_CTRL,
    Key::A,
    Key::S,
    Key::D,
    Key::F,
    Key::G,
    Key::H,
    Key::J,
    Key::K,
    Key::L,
    Key::SEMICOLON,
    Key::APOSTROPHE,
    Key::BACKTICK,
    Key::LEFT_SHIFT,
    Key::BACKSLASH,
    Key::Z,
    Key::X,
    Key::C,
    Key::V,
    Key::B,
    Key::N,
    Key::M,
    Key::COMMA,
    Key::DOT,
    Key::FORWARD_SLASH,
    Key::RIGHT_SHIFT,
    Key::NP_ASTERISK,
    Key::LEFT_ALT,
    Key::SPACE,
    Key::CAPS_LOCK,
    Key::F1,
    Key::F2,
    Key::F3,
    Key::F4,
    Key::F5,
    Key::F6,
    Key::F7,
    Key::F8,
    Key::F9,
    Key::F10,
    Key::NUM_LOCK,
    Key::SCRLK,
    Key::NP_SEVEN,
    Key::NP_EIGHT,
    Key::NP_NINE,
    Key::NP_HYPHEN,
    Key::NP_FOUR,
    Key::NP_FIVE,
    Key::NP_SIX,
    Key::NP_PLUS,
    Key::NP_ONE,
    Key::NP_TWO,
    Key::NP_THREE,
    Key::NP_ZERO,
    Key::NP_DOT,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::F11,
    Key::F12
};

static constexpr Key g_multiByteKeys[0x6E] = {
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::MM_PREVIOUS_TRACK,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::MM_NEXT_TRACK,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::NP_ENTER,
    Key::RIGHT_CTRL,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::MM_MUTE,
    Key::UNKNOWN, // MM_CALCULATOR
    Key::MM_PLAY,
    Key::UNKNOWN,
    Key::MM_STOP,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::MM_VOLUME_DOWN,
    Key::UNKNOWN,
    Key::MM_VOLUME_UP,
    Key::UNKNOWN,
    Key::MM_BROWSER_HOME, // WWW HOME
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::NP_FORWARD_SLASH,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::RIGHT_ALT,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::HOME,
    Key::UP_ARROW,
    Key::PAGE_UP,
    Key::UNKNOWN,
    Key::LEFT_ARROW,
    Key::UNKNOWN,
    Key::RIGHT_ARROW,
    Key::UNKNOWN,
    Key::END,
    Key::DOWN_ARROW,
    Key::PAGE_DOWN,
    Key::INSERT,
    Key::DELETE,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::LEFT_SUPER, // LEFT GUI
    Key::RIGHT_SUPER, // RIGHT GUI
    Key::CONTEXT_MENU, // "APPS"?
    Key::MM_POWER,
    Key::MM_SLEEP,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::UNKNOWN,
    Key::MM_WAKE,
    Key::UNKNOWN,
    Key::MM_BROWSER_SEARCH,
    Key::MM_BROWSER_FAVOURITES,
    Key::MM_BROWSER_REFRESH,
    Key::MM_BROWSER_STOP,
    Key::MM_BROWSER_FORWARD,
    Key::MM_BROWSER_BACK,
    Key::MM_MY_COMPUTER,
    Key::MM_EMAIL,
    Key::MM_MEDIA_SELECT
};

uint32_t PS2Keyboard_HandleIRQ(void* ctx) {
    PS2Keyboard* kbd = static_cast<PS2Keyboard*>(ctx);
    if (ctx == nullptr)
        return 0;
    return kbd->HandleIRQ() ? 1 : 0;
}

PS2Keyboard::PS2Keyboard(PS2Controller* controller, bool channel) :  PS2Device(InputDeviceType::Keyboard, controller, channel), m_state(State::NORMAL) {

}

PS2Keyboard::~PS2Keyboard() {

}

int PS2Keyboard::Init() {
    if (Input::g_EventManager == nullptr)
        return -EIO;

    HAL_RegisterLegacyIntHandler(PS2KEYBOARD_INT, &PS2Keyboard_HandleIRQ, this);

    m_eventSourceID = Input::g_EventManager->RegisterEventSource(Input::EventSourceType::Keyboard, this);

    return 0;
}

int PS2Keyboard::Init(DeviceManager::DeviceInfo* info) {
    if (info == nullptr)
        return -EIO;

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

bool PS2Keyboard::HandleIRQ() {
    if (!DataAvailable())
        return false;

    uint8_t code = ReadData();

    if (Input::g_EventManager == nullptr)
        return true; // clear the pending data, but don't do anything with it

    switch (m_state) {
    case E0:
        if (code == 0x2A || code == 0xB7) {
            m_state = State::PRINT_SCREEN;
            return true;
        }
        break;
    case PRINT_SCREEN:
        if (code == 0xE0) {
            m_state = State::E0_REPEAT;
            return true;
        }
        m_state = State::NORMAL;
        break;
    case E0_REPEAT: {
        if (code != 0x37 && code != 0xAA) {
            m_state = State::NORMAL;
            return true; // garbage byte: reset to normal
        }

        Input::Event e = {Input::EventType::KeyboardKey, m_eventSourceID, {}};
        e.eventData.keyboardEvent.key = Key::PRTSC;
        e.eventData.keyboardEvent.released = code == 0xAA;

        Input::g_EventManager->RaiseEvent(e);
        m_state = State::NORMAL;
        return true;
    }
    case E1:
        if (code == 0x1D)
            m_state = State::PAUSE1;
        return true;
    case PAUSE1:
        if (code == 0x45)
            m_state = State::PAUSE2;
        return true;
    case PAUSE2:
        if (code == 0xE1)
            m_state = State::E1_REPEAT;
        return true;
    case E1_REPEAT:
        if (code == 0x9D)
            m_state = State::PAUSE3;
        return true;
    case PAUSE3:
        if (code == 0xC5) {
            Input::Event e = {Input::EventType::KeyboardKey, m_eventSourceID, {}};
            e.eventData.keyboardEvent.key = Key::PAUSE;
            e.eventData.keyboardEvent.released = false;
            Input::g_EventManager->RaiseEvent(e);
            e.eventData.keyboardEvent.released = true;
            Input::g_EventManager->RaiseEvent(e);
        }

        m_state = State::NORMAL;
        return true;
    case NORMAL:
        if (code == 0xE0)
            m_state = State::E0;
        else if (code == 0xE1)
            m_state = State::E1;
        else
            break;
        return true;
    }

    Input::Event e = {Input::EventType::KeyboardKey, m_eventSourceID, {}};
    e.eventData.keyboardEvent.released = code & (1 << 7);

    uint8_t key = code & ~(1 << 7);
    if (m_state == State::NORMAL && key <= 0x58) {
        e.eventData.keyboardEvent.key = g_singleByteKeys[key];
        Input::g_EventManager->RaiseEvent(e);
    } else if (m_state == State::E0 && key <= 0x6D) {
        e.eventData.keyboardEvent.key = g_multiByteKeys[key];
        Input::g_EventManager->RaiseEvent(e);
    }

    m_state = State::NORMAL;
    return true;
}
