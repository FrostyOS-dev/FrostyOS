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

#include "Panic.hpp"
#include "ArchDefs.h"

#include <stdio.h>

char const* g_x86_64_PanicReason;

x86_64_Registers g_x86_64_PanicRegisters;

extern "C" [[noreturn]] void x86_64_Panic(const char* message) {
    x86_64_DisableInterrupts();

    if (message == nullptr)
        message = g_x86_64_PanicReason;

    // all to debug first
    dbgputs("KERNEL PANIC!\n");
    dbgprintf("%s\n", message);
    
    
    // now to stdout
    puts("KERNEL PANIC!\n");
    printf("%s\n", message);


    while (true) {
        __asm__ volatile ("hlt");
    }
}