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

#ifndef _x86_64_TASK_HPP
#define _x86_64_TASK_HPP

#include "../ArchDefs.h"

#include <stdint.h>

class Thread;

extern "C" {

// Switch kernel task. Does not save anything or return to the caller.
[[noreturn]] void x86_64_KernelSwitchTask(const struct x86_64_Registers* regs);

[[noreturn]] void x86_64_PrepCurrentThreadExit(Thread* thread, uint64_t newStack, bool (Thread::*func)(bool), bool arg);

[[noreturn]] void x86_64_Halt();

}

#endif /* _x86_64_TASK_HPP */