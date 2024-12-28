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

#include "PageTables.hpp"

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

#include <assert.h>
#include <string.h>

#include <stdio.h> // temp

uint64_t x86_64_GetPhysicalAddress(x86_64_Level4Table* Table, uint64_t VirtualAddress) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;
    uint64_t PML1Index = (VirtualAddress >> 12) & 0x1FF;

    if (!Table->entries[PML4Index].Present)
        return 0;

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present)
        return 0;
    if (PML3Table->entries[PML3Index].PageSize)
        return ((uint64_t)PML3Table->entries_1GB[PML3Index].Base << 30) | (VirtualAddress & 0x3FFF'FFFF);

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    if (!PML2Table->entries[PML2Index].Present)
        return 0;
    if (PML2Table->entries[PML2Index].PageSize)
        return ((uint64_t)PML2Table->entries_2MB[PML2Index].Base << 21) | (VirtualAddress & 0x1FFFFF);

    x86_64_Level1Table* PML1Table = (x86_64_Level1Table*)to_HHDM((void*)(PML2Table->entries[PML2Index].Base << 12));
    if (!PML1Table->entries[PML1Index].Present)
        return 0;

    return PML1Table->entries[PML1Index].Base << 12 | (VirtualAddress & 0xFFF);
}

/* mapping flags for standard mappings are broken up into multiple parts:
 * 0-11: flags
 * 12-15: ignored
 * 16-27: high flags
 * 28-31: ignored
 */

void x86_64_MapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;
    uint64_t PML1Index = (VirtualAddress >> 12) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }
    else {
        assert(!PML3Table->entries[PML3Index].PageSize);
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML3Table->entries[PML3Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    if (!PML2Table->entries[PML2Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML2Entry* entry = (x86_64_PML2Entry*)&temp;
        PML2Table->entries[PML2Index] = *entry;
    }
    else {
        assert(!PML2Table->entries[PML2Index].PageSize);
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML2Table->entries[PML2Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML2Entry* entry = (x86_64_PML2Entry*)&temp;
        PML2Table->entries[PML2Index] = *entry;
    }

    x86_64_Level1Table* PML1Table = (x86_64_Level1Table*)to_HHDM((void*)(PML2Table->entries[PML2Index].Base << 12));
    uint64_t temp = (Flags & 0xFFF) | (uint64_t)(PhysicalAddress) | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML1Entry* entry = (x86_64_PML1Entry*)&temp;
    PML1Table->entries[PML1Index] = *entry;
}

void x86_64_RemapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;
    uint64_t PML1Index = (VirtualAddress >> 12) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }
    else {
        if (PML3Table->entries[PML3Index].PageSize)
            return;
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML3Table->entries[PML3Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    if (!PML2Table->entries[PML2Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML2Entry* entry = (x86_64_PML2Entry*)&temp;
        PML2Table->entries[PML2Index] = *entry;
    }
    else {
        if (PML2Table->entries[PML2Index].PageSize)
            return;
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML2Table->entries[PML2Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML2Entry* entry = (x86_64_PML2Entry*)&temp;
        PML2Table->entries[PML2Index] = *entry;
    }

    x86_64_Level1Table* PML1Table = (x86_64_Level1Table*)to_HHDM((void*)(PML2Table->entries[PML2Index].Base << 12));
    uint64_t temp = (Flags & 0xFFF) | (uint64_t)(PML1Table->entries[PML1Index].Base << 12) | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML1Entry* entry = (x86_64_PML1Entry*)&temp;
    PML1Table->entries[PML1Index] = *entry;
}

void x86_64_UnmapPage(x86_64_Level4Table* Table, uint64_t VirtualAddress) {
    return; // temp
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;
    uint64_t PML1Index = (VirtualAddress >> 12) & 0x1FF;

    if (!Table->entries[PML4Index].Present)
        return;

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present)
        return;
    if (PML3Table->entries[PML3Index].PageSize)
        return;

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    if (!PML2Table->entries[PML2Index].Present)
        return;
    if (PML2Table->entries[PML2Index].PageSize)
        return;

    x86_64_Level1Table* PML1Table = (x86_64_Level1Table*)to_HHDM((void*)(PML2Table->entries[PML2Index].Base << 12));
    memset(&(PML1Table->entries[PML1Index]), 0, sizeof(x86_64_PML1Entry));

    return; // temp

    for (uint64_t i = 0; i < 512; i++) {
        if (PML1Table->entries[i].Present)
            return;
    }

    printf("Unmapping PML1Table\n");

    g_PMM->FreePage(HHDM_to_phys((void*)PML1Table));
    memset(&(PML2Table->entries[PML2Index]), 0, sizeof(x86_64_PML2Entry));

    for (uint64_t i = 0; i < 512; i++) {
        if (PML2Table->entries[i].Present)
            return;
    }

    printf("Unmapping PML2Table\n");

    g_PMM->FreePage(HHDM_to_phys((void*)PML2Table));
    memset(&(PML3Table->entries[PML3Index]), 0, sizeof(x86_64_PML3Entry));

    for (uint64_t i = 0; i < 512; i++) {
        if (PML3Table->entries[i].Present)
            return;
    }

    printf("Unmapping PML3Table\n");

    g_PMM->FreePage(HHDM_to_phys((void*)PML3Table));
    memset(&(Table->entries[PML4Index]), 0, sizeof(x86_64_PML4Entry));
}


/* mapping flags for 2MiB and 1GiB mappings are broken up into multiple parts:
 * 0-12: flags
 * 13-15: ignored
 * 16-27: high flags
 * 28-31: ignored
 */

void x86_64_Map2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }
    else {
        assert(!PML3Table->entries[PML3Index].PageSize);
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML3Table->entries[PML3Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    uint64_t temp = (Flags & 0x1FFF) | (uint64_t)PhysicalAddress | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML2Entry_2MB* entry = (x86_64_PML2Entry_2MB*)&temp;
    entry->PageSize = 1;
    PML2Table->entries_2MB[PML2Index] = *entry;
}

void x86_64_Remap2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }
    else {
        if (PML3Table->entries[PML3Index].PageSize)
            return;
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)PML3Table->entries[PML3Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML3Entry* entry = (x86_64_PML3Entry*)&temp;
        PML3Table->entries[PML3Index] = *entry;
    }

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    uint64_t temp = (Flags & 0x1FFF) | ((uint64_t)PML2Table->entries_2MB[PML2Index].Base << 21) | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML2Entry_2MB* entry = (x86_64_PML2Entry_2MB*)&temp;
    entry->PageSize = 1;
    PML2Table->entries_2MB[PML2Index] = *entry;
}

void x86_64_Unmap2MiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PML2Index = (VirtualAddress >> 21) & 0x1FF;

    if (!Table->entries[PML4Index].Present)
        return;

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    if (!PML3Table->entries[PML3Index].Present)
        return;
    if (!PML3Table->entries[PML3Index].PageSize)
        return;

    x86_64_Level2Table* PML2Table = (x86_64_Level2Table*)to_HHDM((void*)(PML3Table->entries[PML3Index].Base << 12));
    memset(&(PML2Table->entries_2MB[PML2Index]), 0, sizeof(x86_64_PML2Entry_2MB));

    for (uint64_t i = 0; i < 512; i++) {
        if (PML2Table->entries_2MB[i].Present)
            return;
    }

    g_PMM->FreePage(HHDM_to_phys((void*)PML2Table));
    memset(&(PML3Table->entries[PML3Index]), 0, sizeof(x86_64_PML3Entry));

    for (uint64_t i = 0; i < 512; i++) {
        if (PML3Table->entries[i].Present)
            return;
    }

    g_PMM->FreePage(HHDM_to_phys((void*)PML3Table));
    memset(&(Table->entries[PML4Index]), 0, sizeof(x86_64_PML4Entry));
}

void x86_64_Map1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint64_t PhysicalAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    uint64_t temp = (Flags & 0x1FFF) | (uint64_t)PhysicalAddress | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML3Entry_1GB* entry = (x86_64_PML3Entry_1GB*)&temp;
    entry->PageSize = 1;
    PML3Table->entries_1GB[PML3Index] = *entry;
}

void x86_64_Remap1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress, uint32_t Flags) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;

    if (!Table->entries[PML4Index].Present) {
        void* addr = g_PMM->AllocatePage();
        memset(to_HHDM(addr), 0, 4096);
        uint64_t temp = (Flags & 0xF67) | 0x1 | (uint64_t)addr | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }
    else {
        uint64_t temp = (Flags & 0xF67) | 0x1 | ((uint64_t)Table->entries[PML4Index].Base << 12) | ((uint64_t)(Flags & 0x07FF0000) << (52-16));
        x86_64_PML4Entry* entry = (x86_64_PML4Entry*)&temp;
        Table->entries[PML4Index] = *entry;
    }

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    uint64_t temp = (Flags & 0x1FFF) | ((uint64_t)PML3Table->entries_1GB[PML3Index].Base << 21) | ((uint64_t)(Flags & 0x0FFF0000) << (52-16));
    x86_64_PML3Entry_1GB* entry = (x86_64_PML3Entry_1GB*)&temp;
    entry->PageSize = 1;
    PML3Table->entries_1GB[PML3Index] = *entry;
}

void x86_64_Unmap1GiBPage(x86_64_Level4Table* Table, uint64_t VirtualAddress) {
    uint64_t PML4Index = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PML3Index = (VirtualAddress >> 30) & 0x1FF;

    if (!Table->entries[PML4Index].Present)
        return;

    x86_64_Level3Table* PML3Table = (x86_64_Level3Table*)to_HHDM((void*)(Table->entries[PML4Index].Base << 12));
    memset(&(PML3Table->entries_1GB[PML3Index]), 0, sizeof(x86_64_PML3Entry_1GB));

    for (uint64_t i = 0; i < 512; i++) {
        if (PML3Table->entries_1GB[i].Present)
            return;
    }

    g_PMM->FreePage(HHDM_to_phys((void*)PML3Table));
    memset(&(Table->entries[PML4Index]), 0, sizeof(x86_64_PML4Entry));
}
