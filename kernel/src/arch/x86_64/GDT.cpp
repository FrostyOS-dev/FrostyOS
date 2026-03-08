/*
Copyright (©) 2024-2026  Frosty515

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

#include "GDT.hpp"

#include <string.h>

void x86_64_InitGDT(x86_64_GDTEntry* entries, x86_64_TSS* TSS) {
    memset(entries, 0, sizeof(x86_64_GDTEntry) * x86_64_GDT_ENTRY_COUNT);

    entries[1].Limit0 = 0xFFFF;
    entries[1].Base0 = 0;
    entries[1].Base1 = 0;
    entries[1].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::Execute | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege0 | (uint8_t)x86_64_GDTAccess::Present;
    entries[1].Limit1 = 0xF;
    entries[1].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    entries[1].Base2 = 0;

    entries[2].Limit0 = 0xFFFF;
    entries[2].Base0 = 0;
    entries[2].Base1 = 0;
    entries[2].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege0 | (uint8_t)x86_64_GDTAccess::Present;
    entries[2].Limit1 = 0xF;
    entries[2].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    entries[2].Base2 = 0;

    entries[3].Limit0 = 0xFFFF;
    entries[3].Base0 = 0;
    entries[3].Base1 = 0;
    entries[3].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::Execute | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege3 | (uint8_t)x86_64_GDTAccess::Present;
    entries[3].Limit1 = 0xF;
    entries[3].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    entries[3].Base2 = 0;

    entries[4].Limit0 = 0xFFFF;
    entries[4].Base0 = 0;
    entries[4].Base1 = 0;
    entries[4].Access = (uint8_t)x86_64_GDTAccess::Accessed | (uint8_t)x86_64_GDTAccess::ReadWrite | (uint8_t)x86_64_GDTAccess::NonSystem | (uint8_t)x86_64_GDTAccess::Privilege3 | (uint8_t)x86_64_GDTAccess::Present;
    entries[4].Limit1 = 0xF;
    entries[4].Flags = (uint8_t)x86_64_GDTFlags::LongMode | (uint8_t)x86_64_GDTFlags::Granularity;
    entries[4].Base2 = 0;

    uint64_t TSSBase = reinterpret_cast<uint64_t>(TSS);
    uint32_t TSSLimit = sizeof(x86_64_TSS);

    x86_64_GDTSystemEntry entry;
    memset(&entry, 0, sizeof(x86_64_GDTSystemEntry));
    entry.Limit0 = TSSLimit & 0xFFFF;
    entry.Base0 = TSSBase & 0xFFFF;
    entry.Base1 = (TSSBase >> 16) & 0xFF;
    entry.Access = (uint8_t)x86_64_GDTAccess::TSS_AVAIL | (uint8_t)x86_64_GDTAccess::System | (uint8_t)x86_64_GDTAccess::Privilege0 | (uint8_t)x86_64_GDTAccess::Present;
    entry.Limit1 = (TSSLimit >> 16) & 0xF;
    entry.Base2 = (TSSBase >> 24) & 0xFF;
    entry.Base3 = (TSSBase >> 32) & 0xFFFFFFFF;
    memcpy(&entries[5], &entry, sizeof(x86_64_GDTSystemEntry));

    x86_64_GDTPointer gdtPointer;
    gdtPointer.Limit = sizeof(x86_64_GDTEntry) * x86_64_GDT_ENTRY_COUNT - 1;
    gdtPointer.Base = (uint64_t)entries;

    x86_64_LoadGDT(&gdtPointer, 0x8, 0x10);
}
