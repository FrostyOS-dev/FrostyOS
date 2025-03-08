/*
Copyright (©) 2025  Frosty515

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

#include "TaskUtil.hpp"

#include "../GDT.hpp"
#include "../MSR.h"

#include <string.h>

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
    frame->DS = regs->DS;
    frame->SS = regs->DS;
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
    regs->DS = frame->DS;
    regs->CR3 = frame->CR3;
}

void x86_64_SetThreadRegisters(x86_64_Registers* regs, uint64_t stack, ThreadEntryPoint entryPoint, ProcessMode mode, void* pageMap) {
    memset(regs, 0, sizeof(x86_64_Registers));

    regs->RSP = stack;
    regs->RDI = (uint64_t)entryPoint.Data;
    regs->RIP = (uint64_t)entryPoint.EntryPoint;
    regs->RFLAGS = 0x202;
    regs->CS = mode == ProcessMode::KERNEL ? x86_64_GDT_KERNEL_CODE_SEGMENT : x86_64_GDT_USER_CODE_SEGMENT;
    regs->DS = mode == ProcessMode::KERNEL ? x86_64_GDT_KERNEL_DATA_SEGMENT : x86_64_GDT_USER_DATA_SEGMENT;
    regs->CR3 = (uint64_t)pageMap;
}

void x86_64_SetGSBases(uint64_t kernelBase, uint64_t base) {
    x86_64_WriteMSR(MSR_GS_BASE, base);
    x86_64_WriteMSR(MSR_KERNEL_GS_BASE, kernelBase);
}

void x86_64_SetGSBase(uint64_t base) {
    x86_64_WriteMSR(MSR_GS_BASE, base);
}

uint64_t x86_64_GetGSBase() {
    return x86_64_ReadMSR(MSR_GS_BASE);
}

uint64_t x86_64_GetKernelGSBase() {
    return x86_64_ReadMSR(MSR_KERNEL_GS_BASE);
}