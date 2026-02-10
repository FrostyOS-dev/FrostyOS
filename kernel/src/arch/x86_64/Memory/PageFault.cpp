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

#include "PageFault.hpp"

#include <stdio.h>

#include <Scheduling/Scheduler.hpp>

#include "../Panic.hpp"

bool inPageFault = false;

void x86_64_PageFaultHandler(x86_64_ISR_Frame* frame) {
    x86_64_PageFaultCode code;
    code.present = (frame->ERR & 1) > 0;
    code.write = (frame->ERR & 2) > 0;
    code.user = (frame->ERR & 4) > 0;
    code.reservedWrite = (frame->ERR & 8) > 0;
    code.execute = (frame->ERR & 16) > 0;

    if (!code.reservedWrite && Scheduler::isRunning()) {
        Scheduler::ProcessorState* currentState = GetCurrentProcessorState();
        if (currentState != nullptr && currentState->currentThread != nullptr) {
            Process* process = currentState->currentThread->GetParent();
            if (process != nullptr) {
                VMM::VMM* vmm = process->GetVMM();
                if (vmm != nullptr && vmm->HandlePageFault({code.present, code.write, code.user, code.execute}, frame->CR2))
                    return;
            }
        }
    }


    // not returning to normal code from here, prevent recursive page faults
    if (inPageFault) {
        while (true)
            __asm__ volatile ("hlt");
    }

    inPageFault = true;

    char const* operation;
    if (code.write)
        operation = "write";
    else if (code.execute)
        operation = "execute";
    else if (code.reservedWrite)
        operation = "write reserved metadata";
    else
        operation = "read";

    char buffer[192]; // more than enough just to be safe, max size is around 130 bytes
    snprintf(buffer, 191, "Page fault in %s-mode while trying to %s a %s page at address %p", code.user ? "user" : "kernel", operation, code.present ? "present" : "non-present", frame->CR2);
    
    x86_64_Panic(buffer, frame, true);
}
