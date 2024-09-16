/*
Copyright (©) 2024  Frosty515

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

#ifndef _ACPI_MADT_HPP
#define _ACPI_MADT_HPP

#include <stdint.h>

#include "SDT.hpp"

namespace ACPI {

    struct MADT {
        SDTHeader Header;
        uint32_t LocalAPICAddress;
        uint32_t Flags;
    } __attribute__((packed));

    struct MADTEntry {
        uint8_t Type;
        uint8_t Length;
    } __attribute__((packed));

    struct MADT_LocalAPIC {
        MADTEntry Entry;
        uint8_t ProcessorID;
        uint8_t APICID;
        uint32_t Flags;
    } __attribute__((packed));

    struct MADT_IOAPIC {
        MADTEntry Entry;
        uint8_t IOAPICID;
        uint8_t Reserved;
        uint32_t Address;
        uint32_t GlobalSystemInterruptBase;
    } __attribute__((packed));

    struct MADT_IOAPICInterruptSourceOverride {
        MADTEntry Entry;
        uint8_t Bus;
        uint8_t Source;
        uint32_t GlobalSystemInterrupt;
        uint16_t Flags;
    } __attribute__((packed));

    struct MADT_IOAPICNMI {
        MADTEntry Entry;
        uint8_t NMISource;
        uint8_t Reserved;
        uint16_t Flags;
        uint32_t GSI;
    } __attribute__((packed));

    struct MADT_LocalAPICNMI {
        MADTEntry Entry;
        uint8_t ProcessorID;
        uint16_t Flags;
        uint8_t LINT;
    } __attribute__((packed));

    struct MADT_LocalAPICAddressOverride {
        MADTEntry Entry;
        uint16_t Reserved;
        uint64_t Address;
    } __attribute__((packed));

    struct MADT_Localx2APIC {
        MADTEntry Entry;
        uint16_t Reserved;
        uint32_t x2APICID;
        uint32_t Flags;
        uint32_t ProcessorUID;
    } __attribute__((packed));

    void InitMADT(MADT* madt);

}

#endif /* _ACPI_MADT_HPP */