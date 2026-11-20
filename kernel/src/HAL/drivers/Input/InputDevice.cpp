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

#include "InputDevice.hpp"
#include "InputClient.hpp"
#include "InputEvent.hpp"

#include <errno.h>
#include <string.h>

#include <frostyos/asm/input.h>

#include <Scheduling/Event.hpp>
#include <Scheduling/Process.hpp>

#include <SystemCalls/SystemCall.hpp>

InputDevice::InputDevice() : m_info(nullptr), m_deviceType(InputDeviceType::Unknown), m_eventSourceID(UINT64_MAX) {

}

InputDevice::InputDevice(InputDeviceType type) : m_info(nullptr), m_deviceType(type), m_eventSourceID(UINT64_MAX) {

}

InputDevice::~InputDevice() {

}

int InputDevice::OpenFile(int flags, const Credential&, void** priv) {
    *priv = nullptr;
    if (m_eventSourceID == UINT64_MAX || Input::g_EventManager == nullptr)
        return -ENODEV;

    InputClient* client = new InputClient();
    if (client == nullptr)
        return -ENOMEM;

    int rc = Input::g_EventManager->AttachClient(m_eventSourceID, client);
    if (rc < 0) {
        delete client;
        return rc;
    }

    *priv = client;
    return ESUCCESS;
}

int InputDevice::CloseFile(void* priv, int flags, const Credential&) {
    InputClient* client = static_cast<InputClient*>(priv);
    if (client == nullptr)
        return ESUCCESS;

    if (Input::g_EventManager != nullptr)
        Input::g_EventManager->DetachClient(client);
    delete client;
    return ESUCCESS;
}

int InputDevice::ReadFile(void* priv, void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential&) {
    InputClient* client = static_cast<InputClient*>(priv);
    if (client == nullptr) {
        *bytesRead = 0;
        return -EBADF;
    }
    return client->Read(out, size, flags, bytesRead);
}

int InputDevice::WriteFile(void*, const void*, size_t, int, uint64_t, size_t*, const Credential&) {
    return -EBADF;
}

int InputDevice::IoctlFile(void* priv, size_t op, void* arg, int* result, Process* proc, const Credential& cred) {
    InputClient* client = static_cast<InputClient*>(priv);

    switch (op) {
    case FOSI_GET_DEVICE_INFO: {
        FOSI_DeviceInfo info;
        info.cap = 0;
        switch (m_deviceType) {
        case InputDeviceType::Keyboard:
            info.type = FOSI_DT_KEYBOARD;
            info.cap = FOSI_CAP_KEY;
            break;
        case InputDeviceType::Mouse:
            info.type = FOSI_DT_MOUSE;
            info.cap = FOSI_CAP_KEY | FOSI_CAP_REL;
            break;
        default:
            info.type = 0;
            break;
        }
        if (!UserWrite(arg, &info, sizeof(FOSI_DeviceInfo), proc))
            return EFAULT;
        *result = 0;
        return ESUCCESS;
    }
    case FOSI_SET_CLAIM: {
        if (client == nullptr || Input::g_EventManager == nullptr)
            return EBADF;
        int value;
        if (UserRead(arg, &value, sizeof(int), proc))
            return EFAULT;
        int rc = Input::g_EventManager->SetClaim(client, value != 0, proc, cred);
        if (rc < 0)
            return -rc;
        *result = 0;
        return ESUCCESS;
    }
    case FOSI_SET_CLOCK: {
        if (client == nullptr || Input::g_EventManager == nullptr)
            return EBADF;
        int clock;
        if (!UserRead(arg, &clock, sizeof(int), proc))
            return EFAULT;
        int rc = Input::g_EventManager->SetClientClock(client, clock);
        if (rc < 0)
            return -rc;
        *result = 0;
        return ESUCCESS;
    }
    case FOSI_GET_KEY_STATE: {
        if (client == nullptr || Input::g_EventManager == nullptr)
            return EBADF;
        FOSI_KeyState state;
        int rc = Input::g_EventManager->GetKeyState(client, &state);
        if (rc < 0)
            return -rc;
        if (!UserWrite(arg, &state, sizeof(FOSI_KeyState), proc))
            return EFAULT;
        *result = 0;
        return ESUCCESS;
    }
    }
    return ENOTTY;
}

EventWaitQueue* InputDevice::GetPollQueueFile(void* priv, uint32_t events) {
    InputClient* client = static_cast<InputClient*>(priv);
    return client != nullptr ? client->GetWaitQueue() : nullptr;
}

uint32_t InputDevice::PollReadyFile(void* priv, uint32_t events) {
    InputClient* client = static_cast<InputClient*>(priv);
    return client != nullptr ? client->PollReady(events) : POLLNVAL;
}
