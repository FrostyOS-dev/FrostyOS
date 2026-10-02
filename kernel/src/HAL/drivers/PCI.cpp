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

#include "PCI.hpp"

#include "../ACPI/MCFG.hpp"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <DataStructures/AVLTree.hpp>

#define PCI_GET_KEY(seg, bus, dev, func) (((uint64_t)seg << 24) | ((uint32_t)bus << 16) | ((uint16_t)dev << 8) | func)
#define PCI_GET_SEG(key) ((key >> 24) & 0xFFFF)
#define PCI_GET_BUS(key) ((key >> 16) & 0xFF)
#define PCI_GET_DEV(key) ((key >> 8) & 0xFF)
#define PCI_GET_FUNC(key) (key & 0xFF)

bool (*PCI_Read8)(uint8_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
bool (*PCI_Read16)(uint16_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
bool (*PCI_Read32)(uint32_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
bool (*PCI_Write8)(uint8_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
bool (*PCI_Write16)(uint16_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
bool (*PCI_Write32)(uint32_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) = nullptr;
PCIAccessMethod g_PCIAccessMethod = PCIAccessMethod::Unknown;

AVLTree::wAVLTree<uint64_t, PCIItem*> g_PCIItems;

// Walk the PCI busses, creating a list of devices. Does NOT initialise any devices.
int PCI_Walk() {
    if (PCI_Read8 == nullptr || PCI_Read16 == nullptr || PCI_Read32 == nullptr || PCI_Write8 == nullptr || PCI_Write16 == nullptr || PCI_Write32 == nullptr || g_PCIAccessMethod == PCIAccessMethod::Unknown)
        return false;

    if (g_PCIAccessMethod != PCIAccessMethod::MCFG)
        return false;

    int rc = ESUCCESS;

    MCFG_EnumerateBusses([&rc](uint16_t segment, uint8_t bus, void* alloc) -> bool {
        for (int device = 0; device < 32; device++) {
            uint16_t vendorID = 0xFFFF;
            if (!MCFG_Read16(&vendorID, alloc, bus, device, 0, PCI_CONFIG_VENDOR) || vendorID == 0xFFFF)
                continue; // device doesn't exist
            uint8_t headerType = 0;
            int maxFunc = 8;
            if (!MCFG_Read8(&headerType, alloc, bus, device, 0, PCI_CONFIG_HEADERTYPE) || (headerType & 0x80) == 0)
                maxFunc = 1;
            for (int func = 0; func < maxFunc; func++) {
                uint16_t vendorID = 0xFFFF;
                if (!MCFG_Read16(&vendorID, alloc, bus, device, func, PCI_CONFIG_VENDOR) || vendorID == 0xFFFF)
                    continue; // func doesn't exist
                PCIItem* item = new PCIItem;
                if (item == nullptr) {
                    rc = -ENOMEM;
                    return false;
                }
                item->segment = segment;
                item->bus = bus;
                item->device = device;
                item->function = func;
                MCFG_Read16(&item->deviceID, alloc, bus, device, func, PCI_CONFIG_DEVICEID);
                item->vendorID = vendorID;
                MCFG_Read8(&item->classCode, alloc, bus, device, func, PCI_CONFIG_CLASS);
                MCFG_Read8(&item->subClass, alloc, bus, device, func, PCI_CONFIG_SUBCLASS);
                MCFG_Read8(&item->progIF, alloc, bus, device, func, PCI_CONFIG_PROGIF);
                MCFG_Read8(&item->revision, alloc, bus, device, func, PCI_CONFIG_REVISION);

                g_PCIItems.lock();
                g_PCIItems.Insert(PCI_GET_KEY(segment, bus, device, func), item);
                g_PCIItems.unlock();

                printf("PCI: Found %02hhx:%02hhx.%02hhx %02hhx%02hhx: %04hx:%04hx (rev %2hhx)\n", bus, device, func, item->classCode, item->subClass, item->vendorID, item->deviceID, item->revision);
            }
        }
        return true;
    });

    return rc;
}

// Find the first item that matches the provided criteria, any argument can be set to INT_MAX for it to not be checked.
PCIItem* PCI_MatchItem(int vendor, int device, int classCode, int subClass, int progIF) {
    PCIItem* item = nullptr;
    g_PCIItems.lock();
    g_PCIItems.forEach([&](uint64_t, PCIItem* i) -> bool {
        if ((vendor != INT_MAX && i->vendorID != vendor)
            || (device != INT_MAX && i->deviceID != device)
            || (classCode != INT_MAX && i->classCode != classCode)
            || (subClass != INT_MAX && i->subClass != subClass)
            || (progIF != INT_MAX && i->progIF != progIF))
            return true;
        
        item = i;
        return false;
    });
    g_PCIItems.unlock();
    return item;
}
