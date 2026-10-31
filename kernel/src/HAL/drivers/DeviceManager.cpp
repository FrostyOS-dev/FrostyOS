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

                m_devicesLock.Lock();

                info->id = m_lastID;
                m_lastID++;

                m_devices.insert(info);
                m_devicesLock.Unlock();
                break;
            }
            delete info;
        }
    }
}

int DeviceManager::InitDevices() {
    int rc = 0;
    m_devicesLock.Lock();
    m_devices.Enumerate([&](DeviceInfo* info, void*) -> bool {
        if (info == nullptr || info->dev == nullptr)
            return true;

        rc = info->dev->Init(info);
        return rc == 0;
    }, nullptr);
    m_devicesLock.Unlock();
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

    m_devicesLock.Lock();
    info->id = m_lastID;
    m_lastID++;

    m_devices.insert(info);
    m_devicesLock.Unlock();

    if (outInfo != nullptr)
        *outInfo = info;

    return 0;
}

int DeviceManager::CreateRootItem(CreateParams& params) {
    if (params.info == nullptr || params.name == nullptr)
        return -EINVAL;

    DeviceInfo*& info = params.info;

    if (info->hasVNode)
        return -EEXIST;

    if (m_fs == nullptr)
        return -ENOENT;

    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_Create(".", params.name, params.type, params.mode, m_fs->GetRoot(), m_cred, params.vnode, &vnode);
    if (rc < 0)
        return rc;

    info->hasVNode = true;
    info->rootVNode = static_cast<FS::DevTempFSVNode*>(vnode);
    info->VNodeType = (params.type == FS::VType::DIR);
    
    if (params.setDevice)
        info->rootVNode->SetDevice(info->dev);
    if (params.outVNode != nullptr)
        *params.outVNode = static_cast<FS::DevTempFSVNode*>(vnode);

    return 0;
}

int DeviceManager::CreateSubItem(const char* path, CreateParams& params) {
    return CreateSubItemWithDevice(path, params.info != nullptr ? params.info->dev : nullptr, params);
}

int DeviceManager::CreateSubItemWithDevice(const char* path, Device* dev, CreateParams& params) {
    if (params.info == nullptr || params.name == nullptr || path == nullptr)
        return -EINVAL;

    DeviceInfo*& info = params.info;

    if (m_fs == nullptr || !info->hasVNode || !info->VNodeType || path[0] == '/')
        return -ENOENT;

    FS::VNode* newVNode = nullptr;
    int rc = FS::VFS_Create(path, params.name, params.type, params.mode, info->rootVNode, m_cred, params.vnode, &newVNode);
    if (rc >= 0) {
        if (params.setDevice)
            static_cast<FS::DevTempFSVNode*>(newVNode)->SetDevice(dev);

        if (params.outVNode != nullptr)
            *params.outVNode = static_cast<FS::DevTempFSVNode*>(newVNode);
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
