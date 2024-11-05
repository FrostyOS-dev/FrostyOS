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

#include "kernel.hpp"
#include "KernelSymbols.hpp"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <util.h>

#include <Graphics/VGA.hpp>

#include <Memory/Heap.hpp>
#include <Memory/PageManager.hpp>
#include <Memory/PageTable.hpp>
#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>
#include <Memory/VirtualMemoryAllocator.hpp>

#include <tty/backends/DebugBackend.hpp>
#include <tty/backends/VGABackend.hpp>

#include <tty/TTY.hpp>

#include <HAL/HAL.hpp>

#include <Data-structures/Bitmap.hpp>

#ifdef __x86_64__
#include <arch/x86_64/KernelSymbols.hpp>
#endif


KernelParams g_kernelParams;

VGA g_KVGA;
Colour g_KBackgroundColour;
Colour g_KForegroundColour;

TTYBackendDebug g_KDebugBackend;
TTYBackendVGA g_KVGABackend;

TTY g_KTTY;

PageManager KPM;

SymbolTable KSymTable;

#define MAX_ALLOCATED (256*1024)

struct AllocationData {
    void* ptr;
    uint64_t size;
};

AllocationData allocated[MAX_ALLOCATED];
uint8_t allocated_bitmap_data[MAX_ALLOCATED / 8];

void StartKernel() {
    {
        typedef void (*ctor_fn)();
        ctor_fn* ctors = (ctor_fn*)_ctors_start_addr;
        uint64_t ctors_count = ((uint64_t)_ctors_end_addr - (uint64_t)_ctors_start_addr) / sizeof(ctor_fn);
        for (uint64_t i = 0; i < ctors_count; i++)
            ctors[i]();
    }


    g_KBackgroundColour = Colour(0, 0, 0);
    g_KForegroundColour = Colour(255, 255, 255);

    g_KVGA.Init(&g_kernelParams.framebuffer, g_KBackgroundColour, g_KForegroundColour);

    g_KVGABackend.Init(&g_KVGA);

    g_KTTY.Init();
    g_KTTY.SetBackend(&g_KVGABackend, TTYBackendStream::OUT);
    g_KTTY.SetBackend(&g_KVGABackend, TTYBackendStream::ERR);
    g_KTTY.SetBackend(&g_KDebugBackend, TTYBackendStream::DEBUG);

    g_CurrentTTY = &g_KTTY;

    SetHHDMOffset(g_kernelParams.HHDMStart);
    
    HAL_EarlyInit(g_kernelParams.MemoryMap, g_kernelParams.MemoryMapEntryCount, g_kernelParams.framebuffer.BaseAddress, g_kernelParams.framebuffer.pitch * g_kernelParams.framebuffer.height, g_kernelParams.kernelVirtual, g_kernelParams.kernelPhysical, g_kernelParams.RSDP);
    
    KPM.Initialise(g_KPageTable, g_KVMA, false);
    g_KPM = &KPM;

    InitKernelHeap();
    InitEternalHeap();

    KSymTable.FillFromRawStringData((const char*)g_kernelParams.symbolTable, g_kernelParams.symbolTableSize);
    g_KSymTable = &KSymTable;

    

    // HAL_Stage2();

    uint64_t totalMemAllocated = 0;
    uint64_t netMemAllocated = 0;

    uint64_t allocated_count = 0;
    bool can_allocate = true;
    
    memset(allocated_bitmap_data, 0, MAX_ALLOCATED / 8);
    Bitmap allocated_bitmap = Bitmap(allocated_bitmap_data, MAX_ALLOCATED / 8);
    for (uint64_t i = 0; i < 10'000'000; i++) {
        if ((rand() % 100) >= 50 && can_allocate) {
            uint64_t size = ((rand() % (1024/16)) + 1) * 16;
            // uint64_t size = 4096;
            uint64_t index = 0;
            while (allocated_bitmap[index] && index < MAX_ALLOCATED)
                index++;
            if (index < MAX_ALLOCATED) {
                allocated_bitmap.Set(index, true);
                allocated[index].ptr = kmalloc(size);
                if (allocated[index].ptr == nullptr) {
                    dbgprintf("Failed to allocate region on iteration %lu. totalMemAllocated = %lu, netMemAllocated = %lu\n", i, totalMemAllocated, netMemAllocated);
                    while (true) {}
                    break;
                }
                memset(allocated[index].ptr, 0xCC, size);
                allocated[index].size = size;
                allocated_count++;
                totalMemAllocated += size;
                netMemAllocated += size;
            }
            if (allocated_count == MAX_ALLOCATED)
                can_allocate = false;
        }
        else if (allocated_count > 0) {
            // free a random block
            bool found = false;
            while (!found) {
                uint64_t index = rand() % MAX_ALLOCATED;
                if (allocated_bitmap[index]) {
                    kfree(allocated[index].ptr);
                    allocated_bitmap.Set(index, false);
                    allocated_count--;
                    found = true;
                    can_allocate = true;
                    netMemAllocated -= allocated[index].size;
                }
            }
        }
        // if ((i % 10'000) == 0) {
        dbgprintf("Iteration %lu: totalMemAllocated = %lu, netMemAllocated = %lu\n", i, totalMemAllocated, netMemAllocated);
        // }
    }

    dbgputs("test complete\n");

    puts("Starting FrostyOS\n");
    dbgputs("Starting FrostyOS\n");

    while (true) {
        __asm__ volatile("hlt");
    }
}
