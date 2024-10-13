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

    puts("Starting FrostyOS\n");
    dbgputs("Starting FrostyOS\n");

    while (true) {
        __asm__ volatile("hlt");
    }
}
