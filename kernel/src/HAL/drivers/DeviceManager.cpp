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

DeviceManager::DeviceManager() : m_fs(0), m_cred(), m_lastID(0), m_devices() {

}

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
                info->parent = nullptr;

                info->id = __atomic_fetch_add(&m_lastID, 1, __ATOMIC_RELAXED);
                
                m_devices.lock();
                m_devices.insert(info);
                m_devices.unlock();
                break;
            }
            delete info;
        }
    }
}

int DeviceManager::InitDevices() {
    return InitDeviceList(m_devices);
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
    info->parent = nullptr;

    info->id = __atomic_fetch_add(&m_lastID, 1, __ATOMIC_RELAXED);
    
    m_devices.lock();
    m_devices.insert(info);
    m_devices.unlock();

    if (outInfo != nullptr)
        *outInfo = info;

    return 0;
}

int DeviceManager::AddSubDevice(DeviceInfo* parent, Device* dev, DeviceInfo** outInfo) {
    if (parent == nullptr || dev == nullptr)
        return -EINVAL;

    DeviceInfo* info = new DeviceInfo;
    if (info == nullptr)
        return -ENOMEM;

    info->manager = this;
    info->dev = dev;
    info->type = DeviceType::Device;
    info->parent = parent;

    info->id = __atomic_fetch_add(&m_lastID, 1, __ATOMIC_RELAXED);

    parent->subDevices.lock();
    parent->subDevices.insert(info);
    parent->subDevices.unlock();

    if (outInfo != nullptr)
        *outInfo = info;

    return 0;
}

int DeviceManager::CreateRootItem(const CreateParams& params) {
    if (params.info == nullptr || params.name == nullptr)
        return -EINVAL;

    DeviceInfo* const& info = params.info;

    if (info->hasVNode)
        return -EEXIST;

    if (info->parent != nullptr && !info->parent->hasVNode) // parent must have a vnode
        return -ENOENT;

    if (m_fs == nullptr)
        return -ENOENT;

    FS::VNode* vnode = nullptr;
    FS::VNode* cwd = info->parent == nullptr ? m_fs->GetRoot() : info->parent->rootVNode;
    int rc = FS::VFS_Create(".", params.name, params.type, params.mode, cwd, m_cred, params.vnode, &vnode);
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

int DeviceManager::CreateSubItem(const char* path, const CreateParams& params) {
    return CreateSubItemWithDevice(path, params.info != nullptr ? params.info->dev : nullptr, params);
}

int DeviceManager::CreateSubItemWithDevice(const char* path, Device* dev, const CreateParams& params) {
    if (params.info == nullptr || params.name == nullptr || path == nullptr)
        return -EINVAL;

    DeviceInfo* const& info = params.info;

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

int DeviceManager::InitDeviceList(DeviceList& list) {
    int rc = 0;
    list.lock();
    list.Enumerate([&](DeviceInfo* info, void*) -> bool {
        if (info == nullptr || info->dev == nullptr)
            return true;

        rc = info->dev->Init(info);
        if (rc < 0)
            return true; // don't init sub devices if the parent failed
        rc = InitDeviceList(info->subDevices);
        return true; // ignore init failures
    }, nullptr);
    list.unlock();
    return rc;
}
