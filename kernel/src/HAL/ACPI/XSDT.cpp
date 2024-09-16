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

#include "XSDT.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <Memory/PagingUtil.hpp>

namespace ACPI {

    bool ValidateXSDP(XSDP* xsdp) {
        // first validate the legacy RSDT
        uint8_t Checksum = 0;
        for (size_t i = 0; i < sizeof(RSDP); i++)
            Checksum += ((uint8_t*)xsdp)[i];
        
        if ((Checksum & 0xFF) != 0)
            return false;

        // ensure that it is an XSDT
        if (xsdp->Revision < 2)
            return false;

        // then validate the XSDT
        Checksum = 0;
        for (size_t i = sizeof(RSDP); i < sizeof(XSDP); i++)
            Checksum += ((uint8_t*)xsdp)[i];

        if ((Checksum & 0xFF) != 0)
            return false;

        return true;
    }

    SDTHeader* GetSDT_nocheck(uint64_t Index, XSDT* xsdt) {
        uint64_t Pointer = 0;
        memcpy(&Pointer, &xsdt->Pointers[Index], sizeof(uint64_t));

        return (SDTHeader*)to_HHDM(Pointer);
    }

    SDTHeader* GetSDT(const char* Signature, XSDT* xsdt) {
        uint64_t Count = GetSDTCount(xsdt);
        for (uint64_t i = 0; i < Count; i++) {
            SDTHeader* sdt = GetSDT_nocheck(i, xsdt);
            if (strncmp(sdt->Signature, Signature, 4) == 0)
                return sdt;
        }

        return nullptr;
    }

    SDTHeader* GetSDT(uint64_t Index, XSDT* xsdt) {
        if (Index >= GetSDTCount(xsdt))
            return nullptr;

        return GetSDT_nocheck(Index, xsdt);
    }

    uint64_t GetSDTCount(XSDT* xsdt) {
        return (xsdt->Header.Length - sizeof(SDTHeader)) / sizeof(uint64_t);
    }

}