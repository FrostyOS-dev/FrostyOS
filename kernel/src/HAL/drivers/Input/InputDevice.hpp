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

#ifndef _DRIVERS_INPUT_DEVICE_HPP
#define _DRIVERS_INPUT_DEVICE_HPP

#include "InputEvent.hpp"

#include "../Device.hpp"

#include <DataStructures/Buffer.hpp>

#include <frostyos/asm/input.h>

#include <Scheduling/Event.hpp>

#define MAX_PENDING_INPUT_EVENTS 256

enum class InputDeviceType {
    Keyboard,
    Unknown
};

class InputDevice : public Device {
public:
    InputDevice();
    InputDevice(InputDeviceType type);
    virtual ~InputDevice();

    virtual int Init(DeviceManager::DeviceInfo* info) override = 0;

    virtual int Open(int flags, const Credential& cred) override;
    virtual int Close(int flags, const Credential& cred) override;
    virtual int Read(void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential& cred) override;
    virtual int Write(const void* in, size_t size, int flags, uint64_t, size_t* bytesWritten, const Credential& cred) override;
    virtual int Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) override;

    virtual EventWaitQueue* GetPollQueue(uint32_t events) override;
    virtual uint32_t PollReady(uint32_t events) override;

    inline InputDeviceType GetType() const {
        return m_deviceType;
    }

    inline void SetType(InputDeviceType type) {
        m_deviceType = type;
    }

    virtual DeviceManager::DeviceInfo* GetInfo() {
        return m_info;
    }
    
    void AddUserEvent(const Input::Event& e); // Must ALWAYS come from the same source
    
protected:
    DeviceManager::DeviceInfo* m_info;
    InputDeviceType m_deviceType;
    Input::EventSourceID m_eventSourceID;
    RingBuffer<FOSI_Event, MAX_PENDING_INPUT_EVENTS> m_pendingEvents;
    EventWaitQueue m_queue;
    uint64_t m_users;
};

#endif /* _DRIVERS_INPUT_DEVICE_HPP */