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
#define DEFAULT_TIMESLICE 10

namespace Scheduler {
    
    struct [[gnu::packed]] ProcessorState {
        ProcessorState* self;
        uint64_t id;
        Processor* processor;
        void* kernelStack;
        CPU_Registers registers;
        Thread* currentThread; // must only be modified by the processor that owns this state. Reads must be done with the lock held
        Thread* idleThread;
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
    uint64_t GetProcessorCount();
    
    void AddProcess(Process* process);
    Process* GetProcess(uint64_t pid);
    void RemoveProcess(uint64_t pid);
    
    void ScheduleThread(Thread* thread, ProcessorState* state = nullptr);
    bool AddExistingThread(Thread* thread);

    void CreateIdleThread(); // on the current processor, must be called on BSP first
    bool RemoveThread(Thread* thread, ProcessorState* state = nullptr, bool stop = true, bool lockCPUInfo = true); // If state is nullptr, checks all, otherwise, only checks the provided CPU
    Thread* RemoveCurrentThread(); // ProcessorState and the thread's CPUInfo are assumed to both be locked, and will not be unlocked by this. returns the current thread prior to this being called.

    void TimerTick(uint64_t msSinceLast, void* data);
    bool SaveOnInt(void* data);

    void Yield(bool forceSwitch = false, void* data = nullptr); // data != nullptr means this is run in an interrupt context
    
    void PickNext(bool lockState = true);
    Thread* StealThreadFromOther(ProcessorState* current, int* niceOut = nullptr);
    
    void InitBSPState();
    ProcessorState* InitNewProcessor(Processor* proc);

    [[noreturn]] void Start(bool AP = false);
    void WaitForStart(ProcessorState* state = nullptr);

    bool isRunning();

    [[noreturn]] void IdleTask(void*); // implemented in arch-specific code
    
} // namespace Scheduler

extern "C" Scheduler::ProcessorState* GetCurrentProcessorState(); // implemented in arch-specific code
extern "C" void Scheduler_SaveAndYield(Thread* currentThread); // implemented in arch-specific code

extern "C" void Scheduler_YieldAfterSave(Thread* currentThread, CPU_Registers* regs); // wrapper around Scheduler::Yield that can be called more easily from assembly

#endif /* _SCHEDULER_HPP */