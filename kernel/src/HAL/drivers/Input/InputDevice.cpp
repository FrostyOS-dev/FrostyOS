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
#include "InputEvent.hpp"

#include <errno.h>
#include <string.h>

#include <frostyos/asm/input.h>

#include <Scheduling/Event.hpp>

#include <SystemCalls/SystemCall.hpp>

InputDevice::InputDevice() : m_info(nullptr), m_deviceType(InputDeviceType::Unknown), m_eventSourceID(UINT64_MAX), m_pendingEvents(), m_queue(), m_users(0) {

}

InputDevice::InputDevice(InputDeviceType type) : m_info(nullptr), m_deviceType(type), m_eventSourceID(UINT64_MAX), m_pendingEvents(), m_queue(), m_users(0) {

}

InputDevice::~InputDevice() {

}

int InputDevice::Open(int flags, const Credential& cred) {
    __atomic_fetch_add(&m_users, 1, __ATOMIC_RELAXED);
    return ESUCCESS;
}

int InputDevice::Close(int flags, const Credential& cred) {
    __atomic_fetch_sub(&m_users, 1, __ATOMIC_RELAXED);
    return ESUCCESS;
}

int InputDevice::Read(void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential& cred) {
    *bytesRead = 0;
    if (size < sizeof(FOSI_Event))
        return -ERANGE;

    size_t written = 0;
    while (written < size && (size - written) >= sizeof(FOSI_Event)) {
        FOSI_Event e;
        if (m_pendingEvents.pop(e)) {
            memcpy((void*)((uint64_t)out + written), &e, sizeof(FOSI_Event));
            written += sizeof(FOSI_Event);
            continue;
        }

        if ((flags & O_NONBLOCK) > 0) {
            *bytesRead = written;
            return written > 0 ? ESUCCESS : -EAGAIN;
        }

        EventWaitNode node;
        node.queue = &m_queue;
        node.requestedEvents = POLLIN;
        EventWaitNode* nodes[] = {&node};
        Event::RegisterWaitNodes(nodes, 1);

        if (!m_pendingEvents.empty()) {
            Event::UnregisterWaitNodes(nodes, 1);
            continue;
        }

        Event::BlockOnWaitNodes(nodes, 1, UINT64_MAX);

        Event::UnregisterWaitNodes(nodes, 1);
    }

    *bytesRead = written;
    return ESUCCESS;
}

int InputDevice::Write(const void* in, size_t size, int flags, uint64_t, size_t* bytesWritten, const Credential& cred) {
    return -EBADF;
}

int InputDevice::Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) {
    switch (op) {
    case FOSI_GET_DEVICE_INFO: {
        FOSI_DeviceInfo info;
        info.cap = 0;
        switch (m_deviceType) {
        case InputDeviceType::Keyboard:
            info.type = FOSI_DT_KEYBOARD;
            break;
        default:
            info.type = 0;
            break;
        }
        if (!UserWrite(arg, &info, sizeof(FOSI_DeviceInfo), proc))
            return -EFAULT;
        *result = 0;
        return ESUCCESS;
    }
    }
    return ENOTTY;
}

void InputDevice::AddUserEvent(const Input::Event& e) {
    using namespace Input;

    if (__atomic_load_n(&m_users, __ATOMIC_ACQUIRE) == 0)
        return;

    FOSI_Event event;
    switch (e.type) {
    case EventType::KeyboardKey:
        event.type = FOSI_ET_KEYBOARDKEY;
        event.code = (_u32)e.eventData.keyboardEvent.key;
        event.data = e.eventData.keyboardEvent.released ? 1 : 0;
        break;
    default:
        event.type = 0;
        event.code = 0;
        event.data = 0;
        break;
    }

    if (m_pendingEvents.push(event))
        m_queue.Trigger(POLLIN | POLLRDNORM);
}

EventWaitQueue* InputDevice::GetPollQueue(uint32_t events) {
    return &m_queue;
}

uint32_t InputDevice::PollReady(uint32_t events) {
    uint32_t e = 0;
    if (!m_pendingEvents.empty())
        e |= POLLIN | POLLRDNORM;
    return e & events;
}
