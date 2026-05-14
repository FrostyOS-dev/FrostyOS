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

#include "Process.hpp"
#include "Scheduler.hpp"
#include "Semaphore.hpp"
#include "Thread.hpp"
#include "ThreadList.hpp"

#include <spinlock.h>
#include <string.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/PagingUtil.hpp>
#include <Memory/VMM.hpp>

#include <HAL/HAL.hpp>
#include <HAL/Processor.hpp>
#include <HAL/Time.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Memory/PagingInit.hpp>

#include <arch/x86_64/Scheduling/Task.hpp>
#include <arch/x86_64/Scheduling/TaskUtil.hpp>
#endif

#define MAX_RUN_COUNT 2

namespace Scheduler {

    ProcessorState g_BSPState;
    LinkedList::LockableLinkedList<Process> g_Processes;
    Process g_IdleProcess(ProcessMode::KERNEL, nullptr, 0);
    uint64_t g_LastPID = 0;
    spinlock_t g_PIDLock;
    uint64_t g_isRunning = 0;
    spinlock_t g_ProcessorsLock = SPINLOCK_DEFAULT_VALUE;
    spinlock_t g_StealLock = SPINLOCK_DEFAULT_VALUE;
    uint64_t g_processorCount = 1;

    ThreadList g_deadThreads;
    Semaphore g_deadThreadsSemaphore(0, 1);

    // private functions

    [[noreturn]] void RunThread(Thread* thread, bool interrupt) {
#ifdef __x86_64__
        if (interrupt)
            x86_64_SwitchTask(&thread->GetRegisters());
        else
            x86_64_KernelSwitchTask(&(thread->GetRegisters()));
#endif
    }

    void SaveThreadFromINT(Thread* thread, void* data) {
#ifdef __x86_64__
        x86_64_CopyFromISRFrame((x86_64_ISR_Frame*)data, &(thread->GetMutableRegisters()));
#endif
    }

    // public functions


    void AddProcessor(ProcessorState* processor) {
        // use BSP state as a list head
        ProcessorState* head = &g_BSPState;

        spinlock_acquire(&g_ProcessorsLock);

        while (head->next != nullptr)
            head = head->next;

        head->next = processor;
        processor->prev = head;
        processor->next = nullptr;

        processor->id = head->id + 1;
        processor->isIdle = 0;
        processor->startAllowed = 0;
        memset(processor->runCounts, 0, sizeof(processor->runCounts));
        processor->currentThread = nullptr;
        memset(&(processor->registers), 0, sizeof(processor->registers));

        g_processorCount++;

        spinlock_release(&g_ProcessorsLock);
    }

    ProcessorState* GetProcessor(uint64_t id) {
        spinlock_acquire(&g_ProcessorsLock);
        ProcessorState* head = &g_BSPState;
        while (head != nullptr) {
            if (head->id == id) {
                spinlock_release(&g_ProcessorsLock);
                return head;
            }
            head = head->next;
        }
        spinlock_release(&g_ProcessorsLock);
        return nullptr;
    }

    void RemoveProcessor(uint64_t id) {
        ProcessorState* head = &g_BSPState;
        spinlock_acquire(&g_ProcessorsLock);
        while (head != nullptr) {
            if (head->id == id) {
                if (head->prev != nullptr)
                    head->prev->next = head->next;
                if (head->next != nullptr)
                    head->next->prev = head->prev;
                g_processorCount--;
                spinlock_release(&g_ProcessorsLock);
                return;
            }
            head = head->next;
        }
        spinlock_release(&g_ProcessorsLock);
    }

    uint64_t GetProcessorCount() {
        return g_processorCount;
    }

    void AddProcess(Process* process) {
        spinlock_acquire(&g_PIDLock);
        process->SetPID(g_LastPID++);
        spinlock_release(&g_PIDLock);

        g_Processes.lock();
        g_Processes.insert(process);
        g_Processes.unlock();
    }

    Process* GetProcess(uint64_t pid) {
        struct Data {
            Process* process;
            uint64_t pid;
        } data = {nullptr, pid};
        g_Processes.lock();
        g_Processes.Enumerate([](Process* process, void* data) -> bool {
            Data* d = (Data*)data;
            if (process->GetPID() == d->pid) {
                d->process = process;
                return false;
            }
            return true;
        }, &data);
        g_Processes.unlock();
        return data.process;
    }

    void RemoveProcess(uint64_t pid) {
        Process* process = GetProcess(pid);
        if (process == nullptr)
            return;
        g_Processes.lock();
        g_Processes.remove(process);
        g_Processes.unlock();
    }

    void ScheduleThread(Thread* thread, ProcessorState* state) {
        Process* process = thread->GetParent();
        if (process == nullptr)
            return;

        uint64_t nice = process->GetNice();
        if (nice >= NICE_LEVELS)
            return;

        thread->SetTimeRemaining(DEFAULT_TIMESLICE);
        thread->sleepRemainingTime = 0;

#ifdef __x86_64__
        x86_64_SetThreadRegisters(&(thread->GetMutableRegisters()), thread->GetStack(), thread->GetEntryPoint(), process->GetMode(), from_HHDM(g_KernelRootPageTable));
#endif

        if (state == nullptr)
            state = GetCurrentProcessorState();

        ThreadList& list = state->threads[nice];
        list.lock();
        spinlock_acquire(&thread->GetCPUInfo()->lock);
        thread->GetCPUInfo()->state = state;
        spinlock_release(&thread->GetCPUInfo()->lock);
        list.pushBack(thread);
        list.unlock();
    }

    bool AddExistingThread(Thread* thread) {
        Process* process = thread->GetParent();
        if (process == nullptr)
            return false;

        uint64_t nice = process->GetNice();
        if (nice >= NICE_LEVELS)
            return false;

        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return false;

        thread->SetTimeRemaining(DEFAULT_TIMESLICE);
        thread->sleepRemainingTime = 0;

        ThreadList& list = state->threads[nice];
        list.lock();
        spinlock_acquire(&thread->GetCPUInfo()->lock);
        thread->GetCPUInfo()->state = state;
        spinlock_release(&thread->GetCPUInfo()->lock);
        // Clear the thread's link pointers before adding to ensure clean state
        ThreadListItemInternalData cleanData = {nullptr, nullptr};
        thread->SetThreadListData(cleanData);
        list.pushBack(thread);
        list.unlock();

        return true;
    }

    void CreateIdleThread() {
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return;

        if (state->id == g_BSPState.id)
            g_IdleProcess.SetVMM(VMM::g_KVMM);

        Thread* thread = new Thread({IdleTask, nullptr}, &g_IdleProcess, state->id);
        if (!thread->Init()) {
            delete thread;
            return;
        }

        thread->GetCPUInfo()->state = state;

#ifdef __x86_64__
        x86_64_SetThreadRegisters(&(thread->GetMutableRegisters()), thread->GetStack(), thread->GetEntryPoint(), ProcessMode::KERNEL, from_HHDM(g_KernelRootPageTable));
#endif

        spinlock_acquire(&state->lock);
        state->idleThread = thread;
        spinlock_release(&state->lock);
    }

    Thread* RemoveCurrentThread(bool lock) {
        ProcessorState* state = GetCurrentProcessorState();
        if (lock)
            spinlock_acquire(&state->lock);
        Thread* thread = state->currentThread;
        state->currentThread = nullptr;
        if (lock)
            spinlock_release(&state->lock);
        return thread;
    }

    bool DeleteThread(Thread* thread) {
        g_deadThreads.lock();
        g_deadThreads.pushBack(thread);
        g_deadThreads.unlock();
        g_deadThreadsSemaphore.Signal();
        return true;
    }

    void SleepCurrentThread(uint64_t ms) {
        if (ms == 0)
            return;
        int intState = Processor::DisableInterrupts();
        Thread* thread = RemoveCurrentThread(true);
        if (thread == nullptr) {
            Processor::EnableInterrupts(intState);
            return;
        }

        thread->sleepRemainingTime = ms;

        Scheduler_SaveAndYield(thread);
        Processor::EnableInterrupts(intState);
    }

    void TimerTick(uint64_t msSinceLast, void* data) {
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return;

        if (state->startAllowed == 0)
            return;

        state->sleepingThreads.lock();
        state->sleepingThreads.Enumerate([](Thread* thread, void* data) -> ThreadList::IteratorDecision {
            uint64_t msSinceLast = *static_cast<uint64_t*>(data);
            ProcessorState* state = GetCurrentProcessorState();
            if (msSinceLast >= thread->sleepRemainingTime) {
                thread->sleepRemainingTime = 0;
                state->sleepingThreads.remove(thread);

                int oldNice = thread->GetParent()->GetNice();

                ThreadList& list = state->threads[oldNice];
                list.lock();
                list.pushBack(thread);
                list.unlock();
            } else
                thread->sleepRemainingTime -= msSinceLast;
            return ThreadList::IteratorDecision::Continue;
        }, &msSinceLast);
        state->sleepingThreads.unlock();

        int oldNice = -1;
        
        Thread* oldThread = state->currentThread;
        if (oldThread != nullptr) {
            SaveThreadFromINT(oldThread, data);
            uint64_t timeRemaining = oldThread->GetTimeRemaining();
            if (msSinceLast >= timeRemaining)
                oldThread->SetTimeRemaining(0);
            else
                oldThread->SetTimeRemaining(timeRemaining - msSinceLast);
            if (oldThread->GetTimeRemaining() > 0 && state->isIdle == 0)
                return;

            if (state->isIdle == 0) {
                oldNice = oldThread->GetParent()->GetNice();

                ThreadList& list = state->threads[oldNice];
                list.lock();
                list.pushBack(oldThread);
                list.unlock();
            }
            state->currentThread = nullptr;
        }

        PickNext(false);

        if (state->currentThread == nullptr)
            PANIC("Scheduler: Nothing to run on timer tick");

        state->currentThread->SetTimeRemaining(DEFAULT_TIMESLICE);

        HAL_EndTimerTick();
        RunThread(state->currentThread, true);
    }

    bool SaveOnInt(void *data) {
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr || state->currentThread == nullptr)
            return false;
        SaveThreadFromINT(state->currentThread, data);
        return true;
    }

    struct YieldData {
        void* data;
        Thread* oldThread;
    };

    void Yield_Internal(uint64_t, void*);

    void Yield(Thread* oldThread, bool forceSwitch, void* data) {
        ProcessorState* state = GetCurrentProcessorState();
        YieldData d = {data, oldThread};
        Processor::SwapStackWithReturn(&Yield_Internal, forceSwitch, &d, state->kernelStack);
    }

    void Yield_Internal(uint64_t forceSwitchInt, void* fullData) {
        ProcessorState* state = GetCurrentProcessorState();
        assert(state != nullptr);

        YieldData* d = static_cast<YieldData*>(fullData);
        bool forceSwitch = forceSwitchInt > 0;
        Thread* oldThread = d->oldThread;
        void* data = d->data;

        __asm__ volatile ("" ::: "memory");

        spinlock_acquire(&state->lock);

        if (oldThread != nullptr) {
            if (oldThread->sleepRemainingTime > 0) {
                state->sleepingThreads.lock();
                state->sleepingThreads.pushBack(oldThread);
                state->sleepingThreads.unlock();
            } else if (oldThread->yieldCallback.func != nullptr)
                oldThread->yieldCallback.func(oldThread, oldThread->yieldCallback.data);
            oldThread = nullptr;
        } else {
            oldThread = state->currentThread;
            if (oldThread != nullptr && forceSwitch) {
                if (data != nullptr)
                    SaveThreadFromINT(oldThread, data);

                int oldNice = oldThread->GetParent()->GetNice();
                ThreadList& list = state->threads[oldNice];
                list.lock();
                list.pushBack(oldThread);
                list.unlock();
                state->currentThread = nullptr;
            }
        }

        if (oldThread == nullptr || forceSwitch)
            PickNext(false);

        if (state->currentThread == nullptr) {
            // No thread was selected to run; release the lock and panic.
            spinlock_release(&(state->lock));
            PANIC("Scheduler::Yield: Nothing to run!");
        }

        state->currentThread->SetTimeRemaining(DEFAULT_TIMESLICE);

        spinlock_release(&(state->lock));
        RunThread(state->currentThread, data != nullptr);
    }

    void PickNext(bool lockState) { // the actual algorithm
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return;

        Thread* thread = nullptr;
        int nice = -1;
        int lastNice = -1;
        
        // go forwards through nice levels
        for (int i = 0; i < NICE_LEVELS; i++) {
            ThreadList& list = state->threads[i];
            list.lock();
            if (list.getCount() == 0 || (state->runCounts[i] >= MAX_RUN_COUNT && i != 0 && thread != nullptr)) {
                list.unlock();
                continue;
            }
            
            if (thread != nullptr)
                state->threads[nice].unlock();
            else if (state->runCounts[i] >= MAX_RUN_COUNT)
                state->runCounts[i] = 0;
            thread = list.getHead();
            nice = i;
        }

        lastNice = nice;

        bool normal = true;
        bool idle = false;

        if (thread == nullptr) {
            thread = StealThreadFromOther(state, &nice);
            if (thread == nullptr) {
                thread = state->idleThread;
                idle = true;
            } else {
                Thread::CPUInfo* info = thread->GetCPUInfo(); // already locked by above function
                info->state = state;
                spinlock_release(&info->lock);
            }
            if (thread == nullptr)
                PANIC("Scheduler: Nothing to run on current CPU");
            normal = false;
        }
        
        if (!idle && nice >= 0)
            state->runCounts[nice]++;
        if (normal)
            state->threads[nice].popFront();
        if (lastNice >= 0)
            state->threads[lastNice].unlock();

        for (int i = nice + 1; i < NICE_LEVELS; i++) {
            if (state->runCounts[i] >= MAX_RUN_COUNT)
                state->runCounts[i] = 0;
        }

        if (lockState)
            spinlock_acquire(&(state->lock));

        state->isIdle = thread == state->idleThread ? 1 : 0;

        state->currentThread = thread;

        if (lockState)
            spinlock_release(&(state->lock));
    }

    Thread* StealThreadFromOther(ProcessorState* current, int* niceOut) {
        spinlock_acquire(&g_StealLock);
        ProcessorState* state;
        for (state = &g_BSPState; state != nullptr; state = state->next) {
            if (state->id == current->id)
                continue;
            
            Thread* thread = nullptr;
            int nice = -1;
            
            // go forwards through nice levels
            for (int i = 0; i < NICE_LEVELS; i++) {
                ThreadList& list = state->threads[i];
                list.lock();
                if (list.getCount() == 0 || (current->runCounts[i] >= MAX_RUN_COUNT && i != 0 && thread != nullptr)) {
                    list.unlock();
                    continue;
                }
                
                if (thread != nullptr)
                    state->threads[nice].unlock();
                else if (current->runCounts[i] >= MAX_RUN_COUNT)
                    current->runCounts[i] = 0;
                thread = list.getHead();
                nice = i;
            }

            if (thread != nullptr) {
                spinlock_acquire(&thread->GetCPUInfo()->lock);
                if (niceOut != nullptr)
                    *niceOut = nice;
                Thread* t2 = state->threads[nice].popFront();
                assert(t2 == thread);
                state->threads[nice].unlock();
                spinlock_release(&g_StealLock);
                return thread;
            } else if (nice >= 0)
                state->threads[nice].unlock();
        }
        spinlock_release(&g_StealLock);
        return nullptr;
    }

    void InitBSPState() {
        g_BSPState.self = &g_BSPState;
        g_BSPState.id = 0;
        g_BSPState.processor = g_BSP;
        g_BSPState.currentThread = nullptr;
        g_BSPState.isIdle = 0;
        g_BSPState.startAllowed = 0;
        for (uint32_t i = 0; i < NICE_LEVELS; i++)
            g_BSPState.runCounts[i] = 0;
    }

    ProcessorState* InitNewProcessor(Processor* proc) {
        ProcessorState* state = new ProcessorState();
        state->self = state;
        state->processor = proc;
        AddProcessor(state);
        return state;
    }

    [[noreturn]] void Start(bool AP) {
        PickNext();
        ProcessorState* current = GetCurrentProcessorState();
        assert(current != nullptr);

        if (!AP) {
            current->startAllowed = 1;
            g_isRunning = 1;

            ProcessorState* state = &g_BSPState;
            for (uint64_t i = 0; i < g_processorCount; i++, state = state->next) {
                if (state == nullptr)
                    break; // shouldn't happen, but just to be safe
                if (state->id == current->id)
                    continue;

                state->startAllowed = 1;
            }

        }

        RunThread(current->currentThread, false);
    }

    void WaitForStart(ProcessorState* state) {
        if (state == nullptr)
            state = GetCurrentProcessorState();

        while (state->startAllowed == 0)
            PAUSE();
    }

    bool isRunning() {
        return g_isRunning > 0;
    }

    [[noreturn]] void HandleDeadThreads(void*) {
        while (true) {
            Thread* thread = nullptr;
            while (true) {
                g_deadThreads.lock();
                thread = g_deadThreads.popFront();
                g_deadThreads.unlock();
                
                if (thread == nullptr)
                    break;

                if (Process* proc = thread->GetParent(); proc != nullptr)
                    proc->RemoveThread(thread);
                thread->Delete();
                if (thread->ShouldDelete())
                    delete thread;
            }
            g_deadThreadsSemaphore.Wait(); // nothing to delete, wait until next is ready
        }
    }

} // namespace Scheduler

[[noreturn]] void Scheduler_YieldAfterSave(Thread* currentThread, CPU_Registers* regs) {
    memcpy(&currentThread->GetMutableRegisters(), regs, sizeof(CPU_Registers));
    Scheduler::Yield(currentThread);
    PANIC("Scheduler::Yield returned");
}
