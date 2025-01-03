/*
Copyright (©) 2024-2025  Frosty515

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
#include "PagingUtil.hpp"

#include <assert.h>
#include <string.h>

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

void* x86_64_GetNextPageTable(void* pageTableEntry, uint64_t index) {
    if (index > 5 || index <= 1) {
        if (index < 1 || index > 5) {
            assert(false);
        }
        return nullptr;
    }

    if (pageTableEntry == nullptr)
        return nullptr;

    if (index == 5 && !x86_64_Is5LevelPagingSupported())
        return nullptr;

    if (index == 3 && ((x86_64_PML3Entry*)pageTableEntry)->PageSize == 1)
        return nullptr;

    if (index == 2 && ((x86_64_PML2Entry*)pageTableEntry)->PageSize == 1)
        return nullptr;


    // no need to cast to a specific entry as the important part of the structure is the same

    if (((x86_64_PML1Entry*)pageTableEntry)->Present == 0) // present bit
        return nullptr;

    uint64_t* entry = (uint64_t*)pageTableEntry;

    return (void*)(*entry & 0x000F'FFFF'FFFF'F000);
}

uint64_t x86_64_GetPhysicalAddress(void* pageTable, uint64_t virtualAddress) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 1)
            return 0;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        if (i == 1) {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            return *entry & 0x000F'FFFF'FFFF'F000;
        }
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            if (i == 3 && ((x86_64_PML3Entry*)pageTableEntry)->PageSize == 1 && ((x86_64_PML3Entry*)pageTableEntry)->Present == 1) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                return *entry & 0x000F'FFFF'C000'0000;
            }

            if (i == 2 && ((x86_64_PML2Entry*)pageTableEntry)->PageSize == 1 && ((x86_64_PML2Entry*)pageTableEntry)->Present == 1) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                return *entry & 0x000F'FFFF'FFE0'0000;
            }

            return 0;
        }
        pageTable = to_HHDM(newPageTable);
        i--;
    }

    return 0;
}

void x86_64_MapPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 1)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            // few options here: out of bounds index (not possible), 1 GiB page, 2 MiB page, 4 KiB page, or missing page table
            // if we have a 1 GiB page or 2 MiB page, we can't map a 4 KiB page
            if ((i == 3 && ((x86_64_PML3Entry*)pageTableEntry)->PageSize == 1 && ((x86_64_PML3Entry*)pageTableEntry)->Present == 1) || (i == 2 && ((x86_64_PML2Entry*)pageTableEntry)->PageSize == 1 && ((x86_64_PML2Entry*)pageTableEntry)->Present == 1))
                return;

            // if we have a 4KiB page, we can map it and exit
            if (i == 1) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                *entry = (physicalAddress & 0x000F'FFFF'FFFF'F000) | (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'0FFF) | 1;
                return;
            }

            // if we don't have a page table, we need to create one
            void* data = to_HHDM(g_PMM->AllocatePage());
            memset(data, 0, 4096);
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = (uint64_t)from_HHDM(data) | ((uint64_t)flags & 0x0000'0F67) | 1;
            newPageTable = data;
        }
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
            newPageTable = to_HHDM(newPageTable);
        }
        pageTable = newPageTable;
        i--;
    }
}

void x86_64_RemapPage(void* pageTable, uint64_t virtualAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 1)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        if (i == 1) {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw_entry = *entry & 0x000F'FFFF'FFFF'F000;
            raw_entry |= (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'0FFF) | 1;
            *entry = raw_entry;
            return;
        }
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr)
            return;
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
        }
        pageTable = to_HHDM(newPageTable);
        i--;
    }
}

void x86_64_UnmapPage(void* pageTable, uint64_t virtualAddress) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;
    uint64_t entryStack[4]; // highest level minus 1

    while (pageTable != nullptr) {
        if (i < 1)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        if (i == 1) {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = 0;
            break;
        }
        entryStack[i - 2] = pageTableEntry;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr)
            return;
        pageTable = to_HHDM(newPageTable);
        i--;
    }

    // loop through the stack and delete any empty page tables
    for (uint64_t i = 0; i < 4; i++) {
        uint64_t* entry = (uint64_t*)entryStack[i];
        if (entry == nullptr)
            return;
        uint64_t* table = (uint64_t*)to_HHDM(*entry & 0x000F'FFFF'FFFF'F000);
        for (uint64_t j = 0; j < 512; j++) {
            if (table[j] & 1)
                return;
        }
        g_PMM->FreePage(from_HHDM(table));
        *entry = 0;
    }
}

void x86_64_Map2MiBPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 2)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            // few options here: out of bounds index (not possible), 1 GiB page, 2 MiB page, or missing page table
            // if we have a 1 GiB page we can't map a 2 MiB page
            if (i == 3 && ((x86_64_PML3Entry*)pageTableEntry)->PageSize == 1 && ((x86_64_PML3Entry*)pageTableEntry)->Present == 1)
                return;

            // if we have a 2 MiB page location, we can map it and exit
            if (i == 2) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                *entry = (physicalAddress & 0x000F'FFFF'FFE0'0000) | (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'1FFF) | 1 | (1 << 7);
                return;
            }

            // if we don't have a page table, we need to create one
            void* data = to_HHDM(g_PMM->AllocatePage());
            memset(data, 0, 4096);
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = (uint64_t)from_HHDM(data) | ((uint64_t)flags & 0x0000'0F67) | 1;
            newPageTable = data;
        }
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
            newPageTable = to_HHDM(newPageTable);
        }
        pageTable = newPageTable;
        i--;
    }
}

void x86_64_Remap2MiBPage(void* pageTable, uint64_t virtualAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 2)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            if (i == 2) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                uint64_t raw_entry = *entry & 0x000F'FFFF'FFE0'0000;
                raw_entry |= (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'1FFF) | 1 | (1 << 7);
                *entry = raw_entry;
            }
            return;
        }
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
        }
        pageTable = to_HHDM(newPageTable);
        i--;
    }
}

void x86_64_Unmap2MiBPage(void* pageTable, uint64_t virtualAddress) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;
    uint64_t entryStack[3]; // highest level minus 2

    while (pageTable != nullptr) {
        if (i < 2)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        if (i == 2) {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = 0;
            break;
        }
        entryStack[i - 3] = pageTableEntry;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr)
            return;
        pageTable = to_HHDM(newPageTable);
        i--;
    }

    // loop through the stack and delete any empty page tables
    for (uint64_t i = 0; i < 3; i++) {
        uint64_t* entry = (uint64_t*)entryStack[i];
        if (entry == nullptr)
            return;
        uint64_t* table = (uint64_t*)to_HHDM(*entry & 0x000F'FFFF'FFFF'F000);
        for (uint64_t j = 0; j < 512; j++) {
            if (table[j] & 1)
                return;
        }
        g_PMM->FreePage(from_HHDM(table));
        *entry = 0;
    }
}

void x86_64_Map1GiBPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 3)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            // few options here: out of bounds index (not possible), 1 GiB page, or missing page table

            // if we have a 1 GiB page location, we can map it and exit
            if (i == 3) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                *entry = (physicalAddress & 0x000F'FFFF'C000'0000) | (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'1FFF) | 1 | (1 << 7);
                return;
            }

            // if we don't have a page table, we need to create one
            void* data = to_HHDM(g_PMM->AllocatePage());
            memset(data, 0, 4096);
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = (uint64_t)from_HHDM(data) | ((uint64_t)flags & 0x0000'0F67) | 1;
            newPageTable = data;
        }
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
            newPageTable = to_HHDM(newPageTable);
        }
        pageTable = newPageTable;
        i--;
    }
}

void x86_64_Remap1GiBPage(void* pageTable, uint64_t virtualAddress, uint64_t physicalAddress, uint32_t flags) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;

    while (pageTable != nullptr) {
        if (i < 1)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr) {
            if (i == 3) {
                uint64_t* entry = (uint64_t*)pageTableEntry;
                uint64_t raw_entry = *entry & 0x000F'FFFF'C000'0000;
                raw_entry |= (((uint64_t)flags & 0x0800'0000) << (52 - 16)) | ((uint64_t)flags & 0x0000'1FFF) | 1 | (1 << 7);
                *entry = raw_entry;
            }
            return;
        }
        else {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            uint64_t raw = *entry;
            raw = (raw & 0x000F'FFFF'FFFF'F001) | ((uint64_t)flags & 0x0000'0F67);
            *entry = raw;
        }
        pageTable = to_HHDM(newPageTable);
        i--;
    }
}

void x86_64_Unmap1GiBPage(void* pageTable, uint64_t virtualAddress) {
    uint64_t i = x86_64_Is5LevelPagingSupported() ? 5 : 4;
    uint64_t entryStack[2]; // highest level minus 3

    while (pageTable != nullptr) {
        if (i < 3)
            return;
        uint64_t pageTableEntry = (uint64_t)pageTable + ((virtualAddress >> (12 + (i - 1) * 9)) & 0x1FF) * 8;
        if (i == 3) {
            uint64_t* entry = (uint64_t*)pageTableEntry;
            *entry = 0;
            break;
        }
        entryStack[i - 4] = pageTableEntry;
        void* newPageTable = x86_64_GetNextPageTable((void*)pageTableEntry, i);
        if (newPageTable == nullptr)
            return;
        pageTable = to_HHDM(newPageTable);
        i--;
    }

    // loop through the stack and delete any empty page tables
    for (uint64_t i = 0; i < 2; i++) {
        uint64_t* entry = (uint64_t*)entryStack[i];
        if (entry == nullptr)
            return;
        uint64_t* table = (uint64_t*)to_HHDM(*entry & 0x000F'FFFF'FFFF'F000);
        for (uint64_t j = 0; j < 512; j++) {
            if (table[j] & 1)
                return;
        }
        g_PMM->FreePage(from_HHDM(table));
        *entry = 0;
    }
}
