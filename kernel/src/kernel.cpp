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

#include "kernel.hpp"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <Graphics/VGA.hpp>

#include <HAL/HAL.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

#include <tty/backends/DebugBackend.hpp>
#include <tty/backends/VGABackend.hpp>

#include <tty/TTY.hpp>

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

char Stage2Stack[KERNEL_STACK_SIZE];

Process KProcess(ProcessMode::KERNEL, NICE_LEVELS - 1);
Thread KThread;

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

    HAL_EarlyInit(g_kernelParams.HHDMStart, g_kernelParams.MemoryMap, g_kernelParams.MemoryMapEntryCount, g_kernelParams.pagingMode, g_kernelParams.kernelVirtual, g_kernelParams.kernelPhysical, g_kernelParams.RSDP);

    LinkedList::NodePool_Init();

    Scheduler::AddProcess(&KProcess);

    KThread.Init({Kernel_Stage2, nullptr}, &KProcess);
    KThread.SetStack((uint64_t)Stage2Stack + KERNEL_STACK_SIZE);

    KProcess.SetMainThread(&KThread);

    Scheduler::ScheduleThread(&KThread);

    Scheduler::Start();

    PANIC("Scheduler returned");
}

void Kernel_Stage2(void*) {
    puts("Starting FrostyOS\n");
    dbgputs("Starting FrostyOS\n");

    while (true) {
        __asm__ volatile("hlt");
    }
}