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

#ifndef _SCHEDULER_HPP
#define _SCHEDULER_HPP

#include <stdint.h>

#include <DataStructures/LinkedList.hpp>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>

#include "Thread.hpp"
#include "Process.hpp"
#include "ThreadList.hpp"

#define NICE_LEVELS 16
#define DEFAULT_NICE 8
#define DEFAULT_TIMESLICE 30

namespace Scheduler {
    
    struct [[gnu::packed]] ProcessorState {
        ProcessorState* self;
        uint64_t id;
        Processor* processor;
        CPU_Registers registers;
        Thread* currentThread; // must only be modified by the processor that owns this state. Reads must be done with the lock held
        uint32_t runCounts[NICE_LEVELS]; // share locks with thread lists
        ThreadList threads[NICE_LEVELS];
        uint32_t isIdle;
        uint32_t startAllowed;
        spinlock_t lock;
        
        ProcessorState* next;
        ProcessorState* prev;
    };

    extern ProcessorState g_BSPState;
    
    
    void AddProcessor(ProcessorState* processor);
    ProcessorState* GetProcessor(uint64_t id);
    void RemoveProcessor(uint64_t id);
    
    void AddProcess(Process* process);
    Process* GetProcess(uint64_t pid);
    void RemoveProcess(uint64_t pid);
    
    void ScheduleThread(Thread* thread);
    
    void TimerTick(uint64_t msSinceLast, void* data);
    
    void PickNext(bool lockState = true);
    
    void InitBSPState();

    [[noreturn]] void Start();
    
} // namespace Scheduler

extern "C" Scheduler::ProcessorState* GetCurrentProcessorState(); // implemented in arch-specific code

#endif /* _SCHEDULER_HPP */