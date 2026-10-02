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

#ifndef _HAL_DRIVERS_PCI_HPP
#define _HAL_DRIVERS_PCI_HPP

#include <stdint.h>

#define PCI_CONFIG_VENDOR     0x00
#define PCI_CONFIG_DEVICEID   0x02
#define PCI_CONFIG_COMMAND    0x04
#define PCI_CONFIG_STATUS     0x06
#define PCI_CONFIG_REVISION   0x08
#define PCI_CONFIG_PROGIF     0x09
#define PCI_CONFIG_SUBCLASS   0x0A
#define PCI_CONFIG_CLASS      0x0B
#define PCI_CONFIG_HEADERTYPE 0x0E
#define PCI_CONFIG_CAP        0x34

#define PCI_COMMAND_IO          (1 << 0x0)
#define PCI_COMMAND_MMIO        (1 << 0x1)
#define PCI_COMMAND_BUS_MASTER  (1 << 0x2)
#define PCI_COMMAND_INT_DISABLE (1 << 0xA)

struct PCIItem {
    uint16_t segment;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t deviceID;
    uint16_t vendorID;
    uint8_t classCode;
    uint8_t subClass;
    uint8_t progIF;
    uint8_t revision;
};

enum class PCIAccessMethod {
    MCFG,
    Unknown
};

extern bool (*PCI_Read8)(uint8_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern bool (*PCI_Read16)(uint16_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern bool (*PCI_Read32)(uint32_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern bool (*PCI_Write8)(uint8_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern bool (*PCI_Write16)(uint16_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern bool (*PCI_Write32)(uint32_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
extern PCIAccessMethod g_PCIAccessMethod;

// Walk the PCI busses, creating a list of devices. Does NOT initialise any devices.
int PCI_Walk();

// Find the first item that matches the provided criteria, any argument can be set to INT_MAX for it to not be checked.
PCIItem* PCI_MatchItem(int vendor, int device, int classCode, int subClass, int progIF);

#endif /* _HAL_DRIVERS_PCI_HPP */