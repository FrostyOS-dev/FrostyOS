#include "DeviceManager.hpp"
#include "PCI.hpp"
#include "PCIDevice.hpp"

#include <errno.h>
#include <stdint.h>

#include <fs/VFS.hpp>

#include <fs/DevTempFS/DevTempFS.hpp>

extern "C" {
    extern PCIDriver* __pci_drivers_start;
    extern PCIDriver* __pci_drivers_end;
}

DeviceManager deviceManager;
DeviceManager* g_DeviceManager = &deviceManager;

void DeviceManager::ProbeDevices() {
    PCIDriver** drivers = &__pci_drivers_start;
    uint64_t driversCount = (reinterpret_cast<uint64_t>(&__pci_drivers_end) - reinterpret_cast<uint64_t>(&__pci_drivers_start)) / sizeof(PCIDriver*);

    for (uint64_t i = 0; i < driversCount; i++) {
        PCIDriver* driver = drivers[i];
        if (driver == nullptr || driver->idTable == nullptr)
            continue;

        for (const PCIDeviceID* id = driver->idTable; id->vendorID != 0 || id->classCode != 0; id++) {
            PCIItem* item = PCI_MatchItem(id->vendorID, id->deviceID, id->classCode, id->subClass, id->progIF);
            if (item == nullptr)
                continue;
            DeviceInfo* info = new DeviceInfo; // create the info before initialising the device as it is more likely to fail
            if (info == nullptr)
                continue; // not a lot that can be done
            PCIDevice* dev = nullptr;
            if (driver->Probe(item, &dev) == 0) {
                info->dev = dev;
                info->type = DeviceType::PCIDevice;
                info->manager = this;

                m_devices.lock();

                info->id = m_lastID;
                m_lastID++;

                m_devices.insert(info);
                m_devices.unlock();
                break;
            }
            delete info;
        }
    }
}

int DeviceManager::InitDevices() {
    int rc = 0;
    m_devices.lock();
    m_devices.Enumerate([&](DeviceInfo* info, void*) -> bool {
        if (info == nullptr || info->dev == nullptr)
            return true;

        rc = info->dev->Init(info);
        return rc == 0;
    }, nullptr);
    m_devices.unlock();
    return rc;
}

int DeviceManager::AddDevice(Device* dev, DeviceInfo** outInfo) {
    if (dev == nullptr)
        return -EINVAL;

    DeviceInfo* info = new DeviceInfo;
    if (info == nullptr)
        return -ENOMEM;

    info->manager = this;
    info->dev = dev;
    info->type = DeviceType::Device;

    m_devices.lock();
    info->id = m_lastID;
    m_lastID++;

    m_devices.insert(info);
    m_devices.unlock();

    if (outInfo != nullptr)
        *outInfo = info;

    return 0;
}

int DeviceManager::CreateRootDir(DeviceInfo* info, const char* name) {
    if (info == nullptr || name == nullptr)
        return -EINVAL;

    if (info->hasVNode)
        return -EEXIST;

    if (m_fs == nullptr)
        return -ENOENT;

    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_CreateDir(".", name, m_fs->GetRoot(), m_cred, nullptr, &vnode);
    if (rc < 0)
        return rc;

    info->hasVNode = true;
    info->rootVNode = static_cast<FS::DevTempFSVNode*>(vnode);
    info->VNodeType = true; // directory

    return 0;
}

int DeviceManager::CreateRootFile(DeviceInfo* info, const char* name) {
    if (info == nullptr || name == nullptr)
        return -EINVAL;

    if (info->hasVNode)
        return -EEXIST;

    if (m_fs == nullptr)
        return -ENOENT;

    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_CreateFile(".", name, m_fs->GetRoot(), m_cred, nullptr, &vnode);
    if (rc < 0)
        return rc;

    info->hasVNode = true;
    info->rootVNode = static_cast<FS::DevTempFSVNode*>(vnode);
    info->VNodeType = false; // file
    
    info->rootVNode->SetDevice(info->dev);

    return 0;
}

int DeviceManager::CreateSubDir(DeviceInfo* info, const char* path, const char* name, FS::DevTempFSVNode* vnode, FS::DevTempFSVNode** outVNode) {
    if (info == nullptr || name == nullptr || path == nullptr)
        return -EINVAL;

    if (m_fs == nullptr || !info->hasVNode || !info->VNodeType || path[0] == '/')
        return -ENOENT;

    FS::VNode* newVNode = nullptr;
    int rc = FS::VFS_CreateDir(path, name, info->rootVNode, m_cred, vnode, &newVNode);
    if (rc >= 0 && outVNode != nullptr)
        *outVNode = static_cast<FS::DevTempFSVNode*>(newVNode);
    return rc;
}

int DeviceManager::CreateSubFile(DeviceInfo* info, const char* path, const char* name, bool setDevice, FS::DevTempFSVNode* vnode, FS::DevTempFSVNode** outVNode) {
    if (info == nullptr || name == nullptr || path == nullptr)
        return -EINVAL;

    if (m_fs == nullptr || !info->hasVNode || !info->VNodeType || path[0] == '/')
        return -ENOENT;

    FS::VNode* newVNode = nullptr;
    int rc = FS::VFS_CreateFile(path, name, info->rootVNode, m_cred, vnode, &newVNode);
    if (rc >= 0) {
        if (setDevice)
            static_cast<FS::DevTempFSVNode*>(newVNode)->SetDevice(info->dev);

        if (outVNode != nullptr)
            *outVNode = static_cast<FS::DevTempFSVNode*>(newVNode);
    }
    return rc;
}

void DeviceManager::SetDevFS(FS::DevTempFS* fs) {
    m_fs = fs;
}

FS::DevTempFS* DeviceManager::GetDevFS() {
    return m_fs;
}

void DeviceManager::SetCred(const Credential& cred) {
    m_cred = cred;
}
