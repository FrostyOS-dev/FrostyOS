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

#include "PageFault.hpp"

#include <stdio.h>

#include <HAL/HAL.hpp>

#ifdef __x86_64__
#include "arch/x86_64/Panic.hpp"
#endif

void HandlePageFault(uint64_t address, PageFaultError error, CPU_Registers *regs) {
#ifdef __x86_64__
    uint64_t IP = regs->RIP;
#endif

    char const* operation;
    if (error.Write)
        operation = "write";
    else if (error.InstructionFetch)
        operation = "execute";
    else if (error.ReservedWrite)
        operation = "write reserved metadata";
    else
        operation = "read";

    char buffer[1024];
    snprintf(buffer, 1023, "Page fault in %s-mode at %lp while trying to %s a %s page at address %lp", error.User ? "user" : "kernel", IP, operation, error.Present ? "present" : "non-present", address);
    buffer[1023] = '\0';

#ifdef __x86_64__
    x86_64_Panic(buffer, regs);
#endif

    while (true) {

    }
}
