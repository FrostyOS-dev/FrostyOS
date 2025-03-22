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

#include "PagingInit.hpp"
#include "PagingUtil.hpp"
#include "PAT.hpp"

#include "../KernelSymbols.hpp"

#include <util.h>

#include <HAL/HAL.hpp>

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

PMM KPMM;

void* g_KernelRootPageTable = nullptr;

void x86_64_InitPaging(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, x86_64_PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) {
    SetHHDMOffset(HHDMOffset);
    
    KPMM.Init(memoryMap, memoryMapEntryCount);
    g_PMM = &KPMM;

    if (!x86_64_EnsureNXSupport())
        PANIC("No-Execute bit is not supported!");

    if (x86_64_Is5LevelPagingSupported() && pagingMode != x86_64_PagingMode::_5LVL)
        PANIC("5-level paging is supported, but not enabled!");

    g_KernelRootPageTable = to_HHDM(g_PMM->AllocatePage());
    if (g_KernelRootPageTable == to_HHDM(nullptr))
        PANIC("Failed to allocate kernel root page table!");
    memset(g_KernelRootPageTable, 0, PAGE_SIZE);

    // need to set up the PAT
    if (!x86_64_IsPATSupported()) // should always be supported
        PANIC("Page Attribute Table is not supported!");

    x86_64_InitPAT();

    uint64_t frameBufferEntryIndex = 0;

    // HHDM map any usable, bootloader reclaimable, kernel and modules memory as read-write with default cache policy
    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        if (memoryMap[i]->Type == MEMORY_MAP_ENTRY_FRAMEBUFFER) {
            frameBufferEntryIndex = i;
            continue;
        }

        if (memoryMap[i]->Type != MEMORY_MAP_ENTRY_USABLE && memoryMap[i]->Type != MEMORY_MAP_ENTRY_BOOTLOADER_RECLAIMABLE && memoryMap[i]->Type != MEMORY_MAP_ENTRY_KERNEL_AND_MODULES)
            continue;

        x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, to_HHDM(memoryMap[i]->Base), memoryMap[i]->Base, memoryMap[i]->Length, 0x0800'0003); // read-write, present, no-execute
    }

    // HHDM map the framebuffer as read-write with write-combining cache policy
    x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, to_HHDM(memoryMap[frameBufferEntryIndex]->Base), memoryMap[frameBufferEntryIndex]->Base, memoryMap[frameBufferEntryIndex]->Length, 0x0800'0003 | x86_64_PAT_GetPageMappingFlags(x86_64_PATOffset::WriteCombining)); // read-write, present, no-execute

    // next map the kernel's different sections
    uint64_t aligned_text_start = ALIGN_DOWN((uint64_t)_text_start_addr, PAGE_SIZE);
    uint64_t aligned_text_end = ALIGN_UP((uint64_t)_text_end_addr, PAGE_SIZE);
    uint64_t aligned_rodata_start = ALIGN_DOWN((uint64_t)_rodata_start_addr, PAGE_SIZE);
    uint64_t aligned_rodata_end = ALIGN_UP((uint64_t)_rodata_end_addr, PAGE_SIZE);
    uint64_t aligned_data_start = ALIGN_DOWN((uint64_t)_data_start_addr, PAGE_SIZE);
    uint64_t aligned_data_end = ALIGN_UP((uint64_t)_data_end_addr, PAGE_SIZE);
    uint64_t aligned_bss_start = ALIGN_DOWN((uint64_t)_bss_start_addr, PAGE_SIZE);
    uint64_t aligned_bss_end = ALIGN_UP((uint64_t)_bss_end_addr, PAGE_SIZE);

    // first is the .text, mapped as read-only, execute
    x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, (uint64_t)aligned_text_start, (uint64_t)aligned_text_start - kernelVirtual + kernelPhysical, (uint64_t)aligned_text_end - (uint64_t)aligned_text_start, 0x0000'0001); // read-only, present, execute

    // next is the .rodata, mapped as read-only
    x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, (uint64_t)aligned_rodata_start, (uint64_t)aligned_rodata_start - kernelVirtual + kernelPhysical, (uint64_t)aligned_rodata_end - (uint64_t)aligned_rodata_start, 0x0800'0001); // read-only, present, no-execute

    // next is the .data, mapped as read-write
    x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, (uint64_t)aligned_data_start, (uint64_t)aligned_data_start - kernelVirtual + kernelPhysical, (uint64_t)aligned_data_end - (uint64_t)aligned_data_start, 0x0800'0003); // read-write, present, no-execute

    // finally, .bss, mapped as read-write
    x86_64_MapRegionWithLargestPages(g_KernelRootPageTable, (uint64_t)aligned_bss_start, (uint64_t)aligned_bss_start - kernelVirtual + kernelPhysical, (uint64_t)aligned_bss_end - (uint64_t)aligned_bss_start, 0x0800'0003); // read-write, present, no-execute

    // now load the page table
    x86_64_LoadCR3((uint64_t)from_HHDM(g_KernelRootPageTable));
}
