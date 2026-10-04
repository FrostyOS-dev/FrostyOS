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

#include "kernel.hpp"
#include "KernelSymbols.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <DataStructures/LinkedList.hpp>

#include <fs/TempFS/TempFS.hpp>

#include <fs/InitRAMFS.hpp>
#include <fs/VFS.hpp>

#include <HAL/HAL.hpp>

#include <HAL/drivers/DeviceManager.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

#include <tty/backends/DebugBackend.hpp>
#include <tty/backends/FBConsoleBackend.hpp>

#include <tty/FBConsole.hpp>
#include <tty/TTY.hpp>

#ifdef __x86_64__
#include <arch/x86_64/KernelSymbols.hpp>
#endif

KernelParams g_kernelParams;

Colour g_KBackgroundColour;
Colour g_KForegroundColour;

TTYBackendFBConsole g_KFBConBackend;
TTYBackendDebug g_KDebugBackend;

GraphicalTTY KTTY;

Credential KCred = {0, 0, 0, 0, 0, 0};

Process KProcess(ProcessMode::KERNEL, nullptr, NICE_LEVELS - 1);
Thread KDeadThreadHandler;

FrameBuffer g_KFramebuffer;

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

    assert(0 == FBConsole_EarlyInit(&g_kernelParams.framebuffer));
    g_KFBConBackend.Init(g_KFBConsole);

    KTTY.Init();
    KTTY.SetOutputBackend(&g_KFBConBackend);
    KTTY.SetDebugBackend(&g_KDebugBackend);
    KTTY.SetConsole(g_KFBConsole);

    g_CurrentTTY = &KTTY;
    g_KTTY = &KTTY;

    g_KProcess = &KProcess;
    KProcess.SetCred(KCred);

    HAL_EarlyInit(g_kernelParams.HHDMStart, g_kernelParams.MemoryMap, g_kernelParams.MemoryMapEntryCount, g_kernelParams.pagingMode, g_kernelParams.kernelVirtual, g_kernelParams.kernelPhysical, g_kernelParams.RSDP);

    if (g_kernelParams.symbolTable != nullptr && g_kernelParams.symbolTableSize > 0) {
        SymbolTable* table = new SymbolTable();
        table->SetMemRegion(_kernel_start_addr, _kernel_end_addr);
        table->FillFromRawStringData((const char*)g_kernelParams.symbolTable, g_kernelParams.symbolTableSize);
        g_KSymTable = table;
    }

    KernelStage2Params* params = new KernelStage2Params;
    params->initramfs = g_kernelParams.initramfs;
    params->initramfsSize = g_kernelParams.initramfsSize;

    if (!KProcess.CreateMainThread({Kernel_Stage2, params}))
        PANIC("Failed to create kernel stage 2 main thread");

    if (!KDeadThreadHandler.Init({Scheduler::HandleDeadThreads, nullptr}, g_KProcess))
        PANIC("Failed to init deleted thread handler thread");

    KProcess.AddThread(&KDeadThreadHandler);

    if (!KProcess.Start())
        PANIC("Failed to start kernel stage 2");

    Scheduler::Start();

    PANIC("Scheduler returned");
}

void Kernel_Stage2(void* data) {
    puts("Starting FrostyOS\n");

    KernelStage2Params* params = (KernelStage2Params*)data;

    HAL_Stage2();

    if (FS::VFS_Init() < 0)
        PANIC("VFS Init failed!");

    if (FS::VFS_MountRoot(FS::FSType::TempFS, 0, nullptr, KCred) < 0)
        PANIC("VFS MountRoot failed!");

    printf("VFS root mounted!\n");

    if (params->initramfs != nullptr && params->initramfsSize > 0)
        LoadInitRAMFS(params->initramfs, params->initramfsSize);
    else
        PANIC("No initramfs!");

    int rc = FS::VFS_CreateDir("/", "dev", nullptr, KCred);
    if (rc < 0 && rc != -EEXIST)
        PANIC("Cannot create /dev");

    rc = FS::VFS_Mount(FS::FSType::DevTempFS, "/dev", 0, g_DeviceManager, nullptr, KCred);
    if (rc < 0)
        PANIC("Cannot mount DevTempFS");

    printf("DevTempFS mounted at /dev!\n");

    g_DeviceManager->SetCred(KCred);

    FBConsole* console = nullptr;
    HAL_InitialseDevices(&console);
    if (console != nullptr) {
        KTTY.SetConsole(console);
        g_KFBConBackend.SetConsole(console);
    }

    while (true) {
        __asm__ volatile("hlt");
    }
}
