/*
Copyright (©) 2026  Frosty515

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

#include "ArchDefs.h"
#include "CPUID.h"
#include "GDT.hpp"
#include "MSR.h"
#include "Syscall.hpp"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <HAL/HAL.hpp>

#include <Scheduling/Scheduler.hpp>

#include <SystemCalls/SystemCall.hpp>

extern "C" uint64_t x86_64_SyscallHandler(x86_64_Registers* regs) {
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    Thread* currentThread = state->currentThread;
    memcpy(&currentThread->GetMutableRegisters(), regs, sizeof(CPU_Registers));
    currentThread->SetStack(regs->RSP);

    Processor::EnableInterrupts();

    uint64_t rc = HandleSystemCall(regs->RAX, regs->RDI, regs->RSI, regs->RDX, regs->R8, regs->R9, regs);
    Processor::DisableInterrupts();
    regs->RAX = rc;

    int ret = currentThread->DispatchSignals(regs, currentThread->GetExtraContext());
    assert(ret >= 0);
    if (ret > 0) {// signal ready to be handled
        memcpy(&currentThread->GetMutableRegisters(), regs, sizeof(x86_64_Registers));
        Scheduler::RunThread(currentThread, false, true); // syscall return clobbers registers, so need to properly context switch
    }

    return rc;
}

bool x86_64_InitSyscall() {
    x86_64_CPUIDResult result = x86_64_CPUID(0x80000001, 0);
    if ((result.EDX & (1 << 11)) == 0) // RDX - bit 11: SYSCALL/SYSRET
        return false;

    uint64_t EFER = x86_64_ReadMSR(MSR_EFER);
    EFER |= 1; // bit 0: SYSCALL/SYSRET
    x86_64_WriteMSR(MSR_EFER, EFER);

    uint64_t star = (uint64_t)x86_64_GDT_KERNEL_CODE_SEGMENT << 32 | (uint64_t)(x86_64_GDT_USER_DATA_SEGMENT - 8) << 48;

    x86_64_WriteMSR(MSR_STAR, star);
    x86_64_WriteMSR(MSR_LSTAR, (uint64_t)&x86_64_SyscallEntry);
    x86_64_WriteMSR(MSR_CSTAR, 0);
    x86_64_WriteMSR(MSR_FMASK, 1 << 9); // bit 9: IF

    return true;
}
