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

#ifndef _ACPI_XSDT_HPP
#define _ACPI_XSDT_HPP

#include <stdint.h>

#include "SDT.hpp"

namespace ACPI {

    struct RSDP {
        char Signature[8];
        uint8_t Checksum;
        char OEMID[6];
        uint8_t Revision;
        uint32_t RSDTAddress;
    } __attribute__((packed));

    struct XSDP {
        char Signature[8];
        uint8_t Checksum;
        char OEMID[6];
        uint8_t Revision;
        uint32_t RSDTAddress;

        uint32_t Length;
        uint64_t XSDTAddress;
        uint8_t ExtendedChecksum;
        uint8_t Reserved[3];
    } __attribute__((packed));


    struct XSDT {
        SDTHeader Header;
        struct {
            uint32_t Pointer[2];
        } __attribute__((packed)) Pointers[1];
    } __attribute__((packed));

    bool ValidateXSDP(XSDP* xsdp);

    SDTHeader* GetSDT(const char* Signature, XSDT* xsdt);
    SDTHeader* GetSDT(uint64_t Index, XSDT* xsdt);
    uint64_t GetSDTCount(XSDT* xsdt);

}

#endif /* _ACPI_XSDT_HPP */