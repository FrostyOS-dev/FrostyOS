/*
Copyright (©) 2025-2026  Frosty515

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

#include "Task.hpp"
#include "TaskUtil.hpp"

#include "../ArchDefs.h"
#include "../GDT.hpp"
#include "../MSR.h"
#include "../Processor.hpp"

#include "../Memory/PageMapper.hpp"
#include "../Memory/PagingInit.hpp"

#include <errno.h>
#include <string.h>
#include <util.h>

#include <HAL/Processor.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/PagingUtil.hpp>

#include <Scheduling/Scheduler.hpp>

#include <SystemCalls/Signal.hpp>
#include <SystemCalls/SystemCall.hpp>

void x86_64_CopyToISRFrame(const x86_64_Registers* regs, x86_64_ISR_Frame* frame) {
    frame->RAX = regs->RAX;
    frame->RBX = regs->RBX;
    frame->RCX = regs->RCX;
    frame->RDX = regs->RDX;
    frame->RSP = regs->RSP;
    frame->RBP = regs->RBP;
    frame->RSI = regs->RSI;
    frame->RDI = regs->RDI;
    frame->R8 = regs->R8;
    frame->R9 = regs->R9;
    frame->R10 = regs->R10;
    frame->R11 = regs->R11;
    frame->R12 = regs->R12;
    frame->R13 = regs->R13;
    frame->R14 = regs->R14;
    frame->R15 = regs->R15;
    frame->RIP = regs->RIP;
    frame->RFLAGS = regs->RFLAGS;
    frame->CS = regs->CS;
    frame->SS = regs->SS;
    frame->CR2 = 0;
    frame->CR3 = regs->CR3;
    // INT and ERR get discarded anyway, so no need to clear them
}

void x86_64_CopyFromISRFrame(const x86_64_ISR_Frame* frame, x86_64_Registers* regs) {
    regs->RAX = frame->RAX;
    regs->RBX = frame->RBX;
    regs->RCX = frame->RCX;
    regs->RDX = frame->RDX;
    regs->RSP = frame->RSP;
    regs->RBP = frame->RBP;
    regs->RSI = frame->RSI;
    regs->RDI = frame->RDI;
    regs->R8 = frame->R8;
    regs->R9 = frame->R9;
    regs->R10 = frame->R10;
    regs->R11 = frame->R11;
    regs->R12 = frame->R12;
    regs->R13 = frame->R13;
    regs->R14 = frame->R14;
    regs->R15 = frame->R15;
    regs->RIP = frame->RIP;
    regs->RFLAGS = frame->RFLAGS;
    regs->CS = frame->CS;
    regs->SS = frame->SS;
    regs->CR3 = frame->CR3;
}

void x86_64_SetThreadRegisters(x86_64_Registers* regs, uint64_t stack, ThreadEntryPoint entryPoint, ProcessMode mode, PageMapper* mapper) {
    memset(regs, 0, sizeof(x86_64_Registers));

    x86_64_PageMapper* map = (x86_64_PageMapper*)mapper;
    void* pageMap = from_HHDM(map->GetPageTable());

    regs->RSP = stack;
    regs->RDI = (uint64_t)entryPoint.Data;
    regs->RIP = (uint64_t)entryPoint.EntryPoint;
    regs->RFLAGS = 0x202;
    regs->CS = mode == ProcessMode::KERNEL ? x86_64_GDT_KERNEL_CODE_SEGMENT : (x86_64_GDT_USER_CODE_SEGMENT | 3);
    regs->SS = mode == ProcessMode::KERNEL ? x86_64_GDT_KERNEL_DATA_SEGMENT : (x86_64_GDT_USER_DATA_SEGMENT | 3);
    regs->CR3 = (uint64_t)pageMap;
}

void x86_64_CreateHaltISRFrame(x86_64_ISR_Frame* frame) {
    frame->RSP = 0;
    __asm__ volatile ("mov %%rbp, %0" : "=g"(frame->RBP));
    frame->CR3 = (uint64_t)from_HHDM(g_KernelRootPageTable);
    frame->CS = x86_64_GDT_KERNEL_CODE_SEGMENT;
    frame->SS = x86_64_GDT_KERNEL_DATA_SEGMENT;
    frame->RFLAGS = 0x2;
    frame->RIP = (uint64_t)&x86_64_Halt;
}

void x86_64_SetGSBases(uint64_t kernelBase, uint64_t base) {
    x86_64_WriteMSR(MSR_GS_BASE, base);
    x86_64_WriteMSR(MSR_KERNEL_GS_BASE, kernelBase);
}

void x86_64_SetFSBase(uint64_t base) {
    x86_64_WriteMSR(MSR_FS_BASE, base);
}

void x86_64_SetGSBase(uint64_t base) {
    x86_64_WriteMSR(MSR_GS_BASE, base);
}

void x86_64_SetKernelGSBase(uint64_t base) {
    x86_64_WriteMSR(MSR_KERNEL_GS_BASE, base);
}

uint64_t x86_64_GetFSBase() {
    return x86_64_ReadMSR(MSR_FS_BASE);
}

uint64_t x86_64_GetGSBase() {
    return x86_64_ReadMSR(MSR_GS_BASE);
}

uint64_t x86_64_GetKernelGSBase() {
    return x86_64_ReadMSR(MSR_KERNEL_GS_BASE);
}

int x86_64_SetupSignalFrame(Process* proc, x86_64_Registers* regs, x86_64_ExtraContext* extra, const sigaction_t* act, const sigset_t* blocked, int signum) {
    if (regs == nullptr || proc == nullptr)
        return -ENOSYS;

    x86_64_Processor* processor = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    size_t SIMDSize = processor->GetCPUInfo()->SIMDInfo.XSAVESize;

    // Subtract 128-byte red zone
    uint64_t rsp = regs->RSP - 128;

    rsp -= sizeof(x86_64_SignalFrame);
    rsp -= SIMDSize;
    rsp = ALIGN_DOWN_BASE2(rsp, 8);
    if ((rsp & 8) == 0)
        rsp -= 8;

    // Validate writing to the user stack
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return -ENOSYS;

    if (!vmm->ValidateWrite(reinterpret_cast<void*>(rsp), sizeof(x86_64_SignalFrame) + SIMDSize))
        return -EFAULT;

    x86_64_SignalFrame* frame = static_cast<x86_64_SignalFrame*>(__builtin_alloca(sizeof(x86_64_SignalFrame) + SIMDSize)); // build in kernel memory, copy to user later

    frame->restorer = reinterpret_cast<uint64_t>(act->restorer);
    memcpy(&frame->blockedSignals, blocked, sizeof(sigset_t));
    memcpy(&frame->regs, regs, sizeof(x86_64_Registers));

    regs->RIP = reinterpret_cast<uint64_t>(act->address);
    regs->RSP = rsp;
    regs->RDI = signum; // 1st argument: signal number

    regs->RFLAGS &= ~(1UL << 10); // clear direction flag

    regs->CS = x86_64_GDT_USER_CODE_SEGMENT | 3;
    regs->SS = x86_64_GDT_USER_DATA_SEGMENT | 3;

    memcpy(reinterpret_cast<void*>((reinterpret_cast<uint64_t>(frame) + sizeof(x86_64_SignalFrame))), extra->SIMDSaveRegion, SIMDSize);

    if (!UserWrite(reinterpret_cast<void*>(rsp), frame, sizeof(x86_64_SignalFrame) + SIMDSize, proc, false))
        return -EFAULT;

    return ESUCCESS;
}

int x86_64_RestoreSignalFrame(Process* proc, x86_64_Registers* regs, x86_64_ExtraContext* extra, sigset_t* blocked, uint64_t userFrameAddr) {
    if (regs == nullptr || userFrameAddr == 0)
        return -EINVAL;

    x86_64_Processor* processor = static_cast<x86_64_Processor*>(GetCurrentProcessor());
    size_t SIMDSize = processor->GetCPUInfo()->SIMDInfo.XSAVESize;

    x86_64_SignalFrame* frame = static_cast<x86_64_SignalFrame*>(__builtin_alloca(sizeof(x86_64_SignalFrame) + SIMDSize));

    if (!UserRead(reinterpret_cast<void*>(userFrameAddr), frame, sizeof(x86_64_SignalFrame) + SIMDSize, proc))
        return -EFAULT;

    memcpy(regs, &frame->regs, sizeof(x86_64_Registers));
    memcpy(extra->SIMDSaveRegion, reinterpret_cast<void*>((reinterpret_cast<uint64_t>(frame) + sizeof(x86_64_SignalFrame))), SIMDSize);
    memcpy(blocked, &frame->blockedSignals, sizeof(sigset_t));

    regs->CS = x86_64_GDT_USER_CODE_SEGMENT | 3;
    regs->SS = x86_64_GDT_USER_DATA_SEGMENT | 3;

    return ESUCCESS;
}

// from Scheduling/Scheduler.hpp

namespace Scheduler {
    [[noreturn]] void IdleTask(void*) {
        while (true)
            __asm__ volatile("hlt");
    }
}

extern "C" void Scheduler_PrepForTimerTick(uint64_t msSinceLast, void* data) {
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    x86_64_SwapStackWithReturn(&Scheduler::TimerTick, msSinceLast, data, state->kernelStack);
}
