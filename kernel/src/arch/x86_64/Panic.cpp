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

#include "Panic.hpp"
#include "ArchDefs.h"

#include "interrupts/ISR.hpp"

#include <stdio.h>

char const* g_x86_64_PanicReason;

x86_64_Registers g_x86_64_PanicRegisters;

extern "C" [[noreturn]] void x86_64_Panic(const char* message, void* registers, bool type) {
    x86_64_DisableInterrupts();

    x86_64_ISR_Frame* isrFrame = nullptr;
    if (type)
        isrFrame = (x86_64_ISR_Frame*)registers;
    x86_64_Registers* regs;
    if (type) {
        x86_64_Convert_ISRRegs_To_StandardRegs((x86_64_ISR_Frame*)registers, &g_x86_64_PanicRegisters);
        regs = &g_x86_64_PanicRegisters;
    }
    else
        regs = (x86_64_Registers*)registers;

    if (message == nullptr)
        message = g_x86_64_PanicReason;

    // all to debug first
    dbgputs("KERNEL PANIC!\n");
    if (type)
        dbgputs("Exception: ");
    dbgprintf("%s\n", message);

    dbgprintf("RAX=%016lx  RBX=%016lx  RCX=%016lx  RDX=%016lx\n", regs->RAX, regs->RBX, regs->RCX, regs->RDX);
    dbgprintf("RSI=%016lx  RDI=%016lx  RSP=%016lx  RBP=%016lx\n", regs->RSI, regs->RDI, regs->RSP, regs->RBP);
    dbgprintf("R8 =%016lx  R9 =%016lx  R10=%016lx  R11=%016lx\n", regs->R8 , regs->R9 , regs->R10, regs->R11);
    dbgprintf("R12=%016lx  R13=%016lx  R14=%016lx  R15=%016lx\n", regs->R12, regs->R13, regs->R14, regs->R15);
    dbgprintf("RIP=%016lx  RFL=%016lx\n", regs->RIP, regs->RFLAGS);
    dbgprintf("CS=%04x  DS=%04x", regs->CS, regs->DS);
    if (type /* interrupt */) {
        dbgprintf("  SS=%04x\n", isrFrame->SS);
        dbgprintf("INT=%016lx  ERR=%016lx\n", isrFrame->INT, isrFrame->ERR);
        if (isrFrame->INT == 13 || isrFrame->INT == 14)
            dbgprintf("CR2=%016lx  ", isrFrame->CR2);
    }
    else
        dbgputc('\n');
    dbgprintf("CR3=%016lx\n", regs->CR3);
    
    
    // now to stdout
    puts("KERNEL PANIC!\n");
    if (type)
        puts("Exception: ");
    printf("%s\n", message);

    printf("RAX=%016lx  RBX=%016lx  RCX=%016lx  RDX=%016lx\n", regs->RAX, regs->RBX, regs->RCX, regs->RDX);
    printf("RSI=%016lx  RDI=%016lx  RSP=%016lx  RBP=%016lx\n", regs->RSI, regs->RDI, regs->RSP, regs->RBP);
    printf("R8 =%016lx  R9 =%016lx  R10=%016lx  R11=%016lx\n", regs->R8 , regs->R9 , regs->R10, regs->R11);
    printf("R12=%016lx  R13=%016lx  R14=%016lx  R15=%016lx\n", regs->R12, regs->R13, regs->R14, regs->R15);
    printf("RIP=%016lx  RFL=%016lx\n", regs->RIP, regs->RFLAGS);
    printf("CS=%04x  DS=%04x", regs->CS, regs->DS);
    if (type /* interrupt */) {
        printf("  SS=%04x\n", isrFrame->SS);
        printf("INT=%016lx  ERR=%016lx\n", isrFrame->INT, isrFrame->ERR);
        if (isrFrame->INT == 13 || isrFrame->INT == 14)
            printf("CR2=%016lx  ", isrFrame->CR2);
    }
    else
        putchar('\n');
    printf("CR3=%016lx\n", regs->CR3);

    while (true) {
        __asm__ volatile ("hlt");
    }
}