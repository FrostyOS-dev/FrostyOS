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

#ifndef _x86_64_PROCESSOR_HPP
#define _x86_64_PROCESSOR_HPP

#include <stdint.h>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include <Memory/MemoryMap.hpp>

#include <Scheduling/Scheduler.hpp>

class x86_64_Processor final : public Processor {
public:
    x86_64_Processor(bool BSP);
    ~x86_64_Processor();

    void Init() override;
    void Init(uint64_t HHDMOffset, MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, PagingMode pagingMode, uint64_t kernelVirtual, uint64_t kernelPhysical) override;

    void InitTime() override;
};

extern x86_64_Processor g_x86_64_BSP;

extern "C" Processor* GetCurrentProcessor();
extern "C" Scheduler::ProcessorState* GetCurrentProcessorState();

#endif /* _x86_64_PROCESSOR_HPP */