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

#ifndef _x86_64_IOAPIC_HPP
#define _x86_64_IOAPIC_HPP

#include <stdint.h>

enum class x86_64_IOAPIC_Register {
    ID = 0,
    VER = 1,
    ARB = 2,
    REDTBL_0 = 0x10
};

enum class x86_64_IOAPIC_DeliveryMode {
    Fixed = 0,
    LowestPriority = 1,
    SMI = 2,
    NMI = 4,
    INIT = 5,
    ExtINT = 7
};

struct [[gnu::packed]] x86_64_IOAPIC_RedirectionEntry {
    uint8_t Vector;
    uint8_t DeliveryMode : 3;
    uint8_t DestMode : 1; // 0 = Physical, 1 = Logical
    uint8_t DeliveryStatus : 1; // 1 = Send pending
    uint8_t IntPolarity : 1; // 0 = active high, 1 = active low
    uint8_t RemoteIRR : 1;
    uint8_t TriggerMode : 1; // 0 = edge, 1 = level
    uint8_t Masked : 1;
    uint64_t Reserved : 39;
    uint8_t Destination;
};

class x86_64_IOAPIC {
public:
    x86_64_IOAPIC(uint64_t address, uint32_t GSIBase);

    void Init();

    void WriteRegister(x86_64_IOAPIC_Register reg, uint32_t value);
    uint32_t ReadRegister(x86_64_IOAPIC_Register reg);

    void WriteRegister(uint8_t reg, uint32_t value);
    uint32_t ReadRegister(uint8_t reg);

    void SetRedirectionEntry(uint8_t index, x86_64_IOAPIC_RedirectionEntry entry);
    x86_64_IOAPIC_RedirectionEntry GetRedirectionEntry(uint8_t index);


    uint8_t GetID();
    uint32_t GetGSIBase();
    uint32_t GetMaxGSI();

private:
    uint8_t m_ID;
    uint64_t m_baseAddress;
    uint32_t m_GSIBase;
    uint8_t m_maxRedirectionEntries;
};

x86_64_IOAPIC* x86_64_GetIOAPICForGSI(uint32_t GSI);
void x86_64_IOAPIC_SetRedirection(uint8_t source, uint32_t GSI, bool activeLow, bool level);

void x86_64_SetGSIRedirection(uint8_t source, uint32_t GSI);
uint32_t x86_64_GetGSIFromSource(uint8_t source);

#endif /* _x86_64_IOAPIC_HPP */