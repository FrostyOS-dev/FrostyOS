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

#include <fs/VFS.hpp>

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

#define DEFAULT_DEV_ITEM_MODE 0666

class DeviceManager {
public:
    struct DeviceInfo;

    typedef LinkedList::RearInsertLinkedList<DeviceInfo> DeviceList;

    struct DeviceInfo {
        uint64_t id;
        DeviceManager* manager;
        Device* dev;
        DeviceType type;

        bool hasVNode = false;
        bool VNodeType = false; // false for file, true for dir
        FS::DevTempFSVNode* rootVNode = nullptr;

        DeviceList subDevices;
        DeviceInfo* parent = nullptr;
    };

    struct CreateParams {
        DeviceInfo* info;
        const char* name;
        FS::VType type;
        mode_t mode = DEFAULT_DEV_ITEM_MODE;
        bool setDevice = true;
        FS::DevTempFSVNode* vnode = nullptr;
        FS::DevTempFSVNode** outVNode = nullptr;
    };

    DeviceManager();

    void ProbeDevices();
    int InitDevices();

    int AddDevice(Device* dev, DeviceInfo** outInfo = nullptr);
    int AddSubDevice(DeviceInfo* parent, Device* dev, DeviceInfo** outInfo = nullptr);

    int CreateRootItem(const CreateParams& params);
    int CreateRootDir(const char* name, mode_t mode = DEFAULT_DIR_MODE, FS::DevTempFSVNode** outVNode = nullptr);

    int CreateSymlink(const char* name, DeviceInfo* target, mode_t mode = DEFAULT_SYMLINK_MODE, FS::DevTempFSVNode** outVNode = nullptr, FS::DevTempFSVNode* cwd = nullptr, bool relative = true);

    // For the following 2 functions:
    // Path must be relative within the previously created root dir for the device.
    // If vnode is non-null, it is used as the vnode for the new item, but is treated as uninitialised.
    int CreateSubItem(const char* path, const CreateParams& params);
    int CreateSubItemWithDevice(const char* path, Device* dev, const CreateParams& params);

    void SetDevFS(FS::DevTempFS* fs);
    FS::DevTempFS* GetDevFS();

    void SetCred(const Credential& cred);

private:
    int InitDeviceList(DeviceList& list);

    FS::DevTempFS* m_fs;
    Credential m_cred;

    uint64_t m_lastID; // protected by the devices lock
    DeviceList m_devices;
};

extern DeviceManager* g_DeviceManager;

#endif /* _HAL_DEVICE_MANAGER_HPP */