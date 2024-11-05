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

#include "PagingInit.hpp"
#include "PageTable.hpp"
#include "PageTables.hpp"
#include "PagingUtil.hpp"
#include "PAT.hpp"

#include "../KernelSymbols.hpp"

#include <stdint.h>
#include <string.h>
#include <util.h>

#include <Memory/Heap.hpp>
#include <Memory/MemoryMap.hpp>
#include <Memory/PageTable.hpp>
#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>
#include <Memory/VirtualMemoryAllocator.hpp>

#include <HAL/HAL.hpp>

PMM KPMM;

VirtualMemoryAllocator GlobalVMA; // Global VMA for all of addressable memory
VirtualMemoryAllocator KVMA; // Kernel VMA for general purpose kernel memory
VirtualMemoryAllocator KExecVMA; // Kernel VMA for binary (or executable) loading kernel memory

x86_64_PageTable KPageTable(nullptr);

[[gnu::aligned(0x1000)]] x86_64_Level4Table KLevel4Table;

x86_64_Level4Table* g_Level4Table = nullptr;

bool g_2MiBPages = false;
bool g_1GiBPages = false;

bool canUnmap = false;

void x86_64_InitPaging(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, void* fb_base, uint64_t fb_size, uint64_t kernel_virtual, uint64_t kernel_physical) {
    KPMM.Initialise(memoryMap, memoryMapEntryCount);
    g_PMM = &KPMM;

    if (!x86_64_EnsureNX())
        PANIC("CPU does not support the NX bit");

    // g_2MiBPages = x86_64_Ensure2MBPages();
    // g_1GiBPages = x86_64_Ensure1GBPages();

    if (x86_64_isPATSupported())
        x86_64_InitPAT();

    memset(&KLevel4Table, 0, sizeof(x86_64_Level4Table));

    g_Level4Table = &KLevel4Table;

    void* fb_phys = HHDM_to_phys(fb_base);

    if (x86_64_isPATSupported()) {
        if (((uint64_t)fb_phys + fb_size) < GiB(4)) {
            x86_64_MapRegionWithLargestPages(g_Level4Table, KiB(4), (uint64_t)fb_phys - KiB(4), 0x0800'0003); // Present, Read/Write, No Execute
            x86_64_MapRegionWithLargestPages(g_Level4Table, (uint64_t)fb_phys, fb_size, 0x0800'0003 | x86_64_GetPageFlagsFromPATIndex(x86_64_GetPATIndex(x86_64_PATType::WriteCombining))); // Present, Read/Write, No Execute
            x86_64_MapRegionWithLargestPages(g_Level4Table, (uint64_t)fb_phys + fb_size, GiB(4) - ((uint64_t)fb_phys + fb_size), 0x0800'0003); // Present, Read/Write, No Execute
        }
        else {
            x86_64_MapRegionWithLargestPages(g_Level4Table, KiB(4), GiB(4) - KiB(4), 0x0800'0003); // Present, Read/Write, No Execute
            x86_64_MapRegionWithLargestPages(g_Level4Table, (uint64_t)fb_phys, fb_size, 0x0800'0003 | x86_64_GetPageFlagsFromPATIndex(x86_64_GetPATIndex(x86_64_PATType::WriteCombining))); // Present, Read/Write, No Execute
        }
    }
    else
        x86_64_MapRegionWithLargestPages(g_Level4Table, KiB(4), GiB(4) - KiB(4), 0x0800'0003); // Present, Read/Write, No Execute

    // map all memory map entries above 4GiB that aren't bad memory or framebuffer when PAT is supported
    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        if (memoryMap[i]->type == MEMORY_MAP_BAD_MEMORY)
            continue;
        if (x86_64_isPATSupported() && memoryMap[i]->type == MEMORY_MAP_FRAMEBUFFER)
            continue;

        if ((memoryMap[i]->base + memoryMap[i]->length) <= GiB(4))
            continue;

        uint64_t base = memoryMap[i]->base;
        uint64_t length = memoryMap[i]->length;

        if (base < GiB(4)) {
            length -= GiB(4) - base;
            base = GiB(4);
        }

        x86_64_MapRegionWithLargestPages(g_Level4Table, to_HHDM(base), base, length, 0x0800'0003); // Present, Read/Write, No Execute
    }

    assert(kernel_virtual == 0xFFFFFFFF80000000UL);

    MapKernel(kernel_virtual, kernel_physical);

    x86_64_CR3 cr3;
    memset(&cr3, 0, sizeof(x86_64_CR3));
    uint64_t phys_addr = x86_64_GetPhysicalAddress(g_Level4Table, (uint64_t)g_Level4Table);
    cr3.PML4 = phys_addr >> 12;
    x86_64_LoadCR3(cr3);

    InitVMMHeap();

    GlobalVMA.AddRegion(nullptr, (1UL << (64 - PAGE_SIZE_SHIFT)));
    GlobalVMA.ReservePages(nullptr, 1); // first page
    GlobalVMA.ReservePages((void*)PAGE_SIZE, (1UL << (48 - PAGE_SIZE_SHIFT)) - 1); // user memory
    GlobalVMA.ReservePages((void*)(1UL << 48), (0xFFFF8UL << (44 - PAGE_SIZE_SHIFT))); // non-canonical addresses
    GlobalVMA.ReservePages((void*)(0xFFFF8UL << 44), (1UL << (46 - PAGE_SIZE_SHIFT))); // HHDM
    GlobalVMA.ReservePages((void*)(0xFFFFCUL << 44), (0xFFFFFUL << (44 - PAGE_SIZE_SHIFT))); // general purpose kernel memory
    GlobalVMA.ReservePages((void*)0xFFFFFFFF80000000UL, (1UL << (31 - PAGE_SIZE_SHIFT))); // binary (or executable) loading kernel memory

    KVMA.AddRegion((void*)(0xFFFFCUL << 44), (0xFFFFFUL << (44 - PAGE_SIZE_SHIFT)));

    KExecVMA.AddRegion((void*)0xFFFFFFFF80000000UL, (1UL << (31 - PAGE_SIZE_SHIFT)));
    KExecVMA.ReservePages((void*)0xFFFFFFFF80000000UL, DIV_ROUNDUP((uint64_t)_kernel_end_addr - 0xFFFFFFFF80000000UL, PAGE_SIZE)); // kernel code and data

    g_GlobalVMA = &GlobalVMA;
    g_KVMA = &KVMA;
    g_KExecVMA = &KExecVMA;

    KPageTable.SetTable(g_Level4Table);

    g_KPageTable = &KPageTable;

    canUnmap = true;
}

bool x86_64_is2MiBPagesSupported() {
    return g_2MiBPages;
}

bool x86_64_is1GiBPagesSupported() {
    return g_1GiBPages;
}

void MapKernel(uint64_t kernel_virtual, uint64_t kernel_physical) {
    uint64_t text_start_addr = ALIGN_DOWN(((uint64_t)_text_start_addr), PAGE_SIZE);
    uint64_t text_end_addr = ALIGN_UP(((uint64_t)_text_end_addr), PAGE_SIZE);
    uint64_t rodata_start_addr = ALIGN_DOWN(((uint64_t)_rodata_start_addr), PAGE_SIZE);
    uint64_t rodata_end_addr = ALIGN_UP(((uint64_t)_rodata_end_addr), PAGE_SIZE);
    uint64_t data_start_addr = ALIGN_DOWN(((uint64_t)_data_start_addr), PAGE_SIZE);
    uint64_t data_end_addr = ALIGN_UP(((uint64_t)_data_end_addr), PAGE_SIZE);
    uint64_t bss_start_addr = ALIGN_DOWN(((uint64_t)_bss_start_addr), PAGE_SIZE);
    uint64_t bss_end_addr = ALIGN_UP(((uint64_t)_bss_end_addr), PAGE_SIZE);

    x86_64_MapRegionWithLargestPages(g_Level4Table,   text_start_addr,   text_start_addr - kernel_virtual + kernel_physical,   text_end_addr -   text_start_addr, 0x0000'0001); // Present, Read, Execute
    x86_64_MapRegionWithLargestPages(g_Level4Table, rodata_start_addr, rodata_start_addr - kernel_virtual + kernel_physical, rodata_end_addr - rodata_start_addr, 0x0800'0001); // Present, Read, No Execute
    x86_64_MapRegionWithLargestPages(g_Level4Table,   data_start_addr,   data_start_addr - kernel_virtual + kernel_physical,   data_end_addr -   data_start_addr, 0x0800'0003); // Present, Read/Write, No Execute
    x86_64_MapRegionWithLargestPages(g_Level4Table,    bss_start_addr,    bss_start_addr - kernel_virtual + kernel_physical,    bss_end_addr -    bss_start_addr, 0x0800'0003); // Present, Read/Write, No Execute
}
