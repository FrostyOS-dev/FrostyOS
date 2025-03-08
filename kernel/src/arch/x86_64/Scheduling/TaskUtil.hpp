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

#ifndef _x86_64_TASK_UTIL_HPP
#define _x86_64_TASK_UTIL_HPP

#include <stdint.h>

#include "../ArchDefs.h"

#include "../interrupts/ISR.hpp"

#include <Scheduling/Process.hpp>
#include <Scheduling/Thread.hpp>


void x86_64_CopyToISRFrame(const x86_64_Registers* regs, x86_64_ISR_Frame* frame);
void x86_64_CopyFromISRFrame(const x86_64_ISR_Frame* frame, x86_64_Registers* regs);

void x86_64_SetThreadRegisters(x86_64_Registers* regs, uint64_t stack, ThreadEntryPoint entryPoint, ProcessMode mode, void* pageMap);

void x86_64_SetGSBases(uint64_t kernelBase, uint64_t base);
void x86_64_SetGSBase(uint64_t base);
uint64_t x86_64_GetGSBase();
uint64_t x86_64_GetKernelGSBase();

#endif /* _x86_64_TASK_UTIL_HPP */