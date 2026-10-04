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

#ifndef _HAL_PCI_DEVICE_HPP
#define _HAL_PCI_DEVICE_HPP

#include <limits.h>
#include <stdint.h>

#include "Device.hpp"
#include "PCI.hpp"

#define REGISTER_PCI_DRIVER(driverStruct) __attribute__((section("pci_drivers"), used)) static const PCIDriver* __pci_drv_ptr_##driverStruct = &driverStruct;

struct PCIDeviceID {
    int vendorID = INT_MAX;
    int deviceID = INT_MAX;
    int classCode = INT_MAX;
    int subClass = INT_MAX;
    int progIF = INT_MAX;
};

class PCIDevice : public Device {
public:
    PCIDevice(PCIItem* item) : m_item(item) {}
    virtual ~PCIDevice() {}

    virtual int Init(DeviceManager::DeviceInfo* info) override = 0;

    virtual int Open(int flags, const Credential& cred) override = 0;
    virtual int Close(int flags, const Credential& cred) override = 0;
    virtual int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) override = 0;
    virtual int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) override = 0;
    virtual int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) override = 0;
    virtual int Munmap() override = 0;
    virtual int Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) override = 0;

    inline void SetItem(PCIItem* item) { m_item = item; }
    inline PCIItem* GetItem() { return m_item; }

private:
    PCIItem* m_item;
};

struct PCIDriver {
    const char* name;
    const PCIDeviceID* idTable;

    int (*Probe)(PCIItem* pciItem, PCIDevice** outDev); // Create the device, but do not initialise it
    void (*Remove)(PCIDevice* dev);
};

#endif /* _HAL_PCI_DEVICE_HPP */