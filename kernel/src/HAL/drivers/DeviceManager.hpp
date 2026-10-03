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

#ifndef _HAL_DEVICE_MANAGER_HPP
#define _HAL_DEVICE_MANAGER_HPP

#include <stdint.h>

#include <DataStructures/LinkedList.hpp>

#include <Scheduling/Process.hpp>

class Device;

namespace FS {
    class DevTempFS;
    class DevTempFSVNode;
}

enum class DeviceType {
    PCIDevice,
    Device,
    Invalid
};

class DeviceManager {
public:
    struct DeviceInfo {
        uint64_t id;
        DeviceManager* manager;
        Device* dev;
        DeviceType type;

        bool hasVNode;
        bool VNodeType; // false for file, true for dir
        FS::DevTempFSVNode* rootVNode;
    };

    void ProbeDevices();
    int InitDevices();

    int AddDevice(Device* dev, DeviceInfo** outInfo = nullptr);

    int CreateRootDir(DeviceInfo* info, const char* name);
    int CreateRootFile(DeviceInfo* info, const char* name);

    // For the following 2 functions:
    // Path must be relative within the previously created root dir for the device.
    // If vnode is non-null, it is used as the vnode for the new item, but is treated as uninitialised.

    // Create a subdirectory for a device
    int CreateSubDir(DeviceInfo* info, const char* path, const char* name, FS::DevTempFSVNode* vnode = nullptr, FS::DevTempFSVNode** outVNode = nullptr);

    // Create a sub-file for a device
    int CreateSubFile(DeviceInfo* info, const char* path, const char* name, bool setDevice = true, FS::DevTempFSVNode* vnode = nullptr, FS::DevTempFSVNode** outVNode = nullptr);

    void SetDevFS(FS::DevTempFS* fs);
    FS::DevTempFS* GetDevFS();

    void SetCred(const Credential& cred);

private:
    FS::DevTempFS* m_fs;
    Credential m_cred;

    uint64_t m_lastID; // protected by the device list lock
    LinkedList::LockableLinkedList<DeviceInfo> m_devices;
};

extern DeviceManager* g_DeviceManager;

#endif /* _HAL_DEVICE_MANAGER_HPP */