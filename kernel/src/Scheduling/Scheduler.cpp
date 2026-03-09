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
#include "Thread.hpp"

#include <spinlock.h>
#include <string.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/PagingUtil.hpp>
#include <Memory/VMM.hpp>

#include <HAL/Processor.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Memory/PagingInit.hpp>

#include <arch/x86_64/Scheduling/Task.h>
#include <arch/x86_64/Scheduling/TaskUtil.hpp>
#endif

namespace Scheduler {

    ProcessorState g_BSPState;
    LinkedList::LockableLinkedList<Process> g_Processes;
    Process g_IdleProcess(ProcessMode::KERNEL, nullptr, 0);
    uint64_t g_LastPID = 0;
    spinlock_t g_PIDLock;
    uint64_t g_isRunning = 0;
    spinlock_t g_ProcessorsLock = SPINLOCK_DEFAULT_VALUE;
    uint64_t g_processorCount = 1;

    // private functions

    [[noreturn]] void RunThread(Thread* thread) {
#ifdef __x86_64__
        x86_64_KernelSwitchTask(&(thread->GetRegisters()));
#endif
    }

    void RunThreadOnINTReturn(Thread* thread, Thread* oldThread, void* data) {
#ifdef __x86_64__
        if (oldThread != nullptr)
            x86_64_CopyFromISRFrame((x86_64_ISR_Frame*)data, &(oldThread->GetMutableRegisters()));
        x86_64_CopyToISRFrame(&(thread->GetRegisters()), (x86_64_ISR_Frame*)data);
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

#ifdef __x86_64__
        x86_64_SetThreadRegisters(&(thread->GetMutableRegisters()), thread->GetStack(), thread->GetEntryPoint(), process->GetMode(), from_HHDM(g_KernelRootPageTable));
#endif

        if (state == nullptr)
            state = &g_BSPState;

        ThreadList& list = state->threads[nice];
        list.lock();
        list.pushBack(thread);
        list.unlock();
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

#ifdef __x86_64__
        x86_64_SetThreadRegisters(&(thread->GetMutableRegisters()), thread->GetStack(), thread->GetEntryPoint(), ProcessMode::KERNEL, from_HHDM(g_KernelRootPageTable));
#endif

        spinlock_acquire(&state->lock);
        state->idleThread = thread;
        spinlock_release(&state->lock);
    }

    // Assumes that if state is not the current state, then the CPU is already stopped. If it is, and the thread is running, this assumes the stack is already swapped.
    bool RemoveThreadFromState(Thread* thread, ProcessorState* state) {
        if (thread == state->currentThread) {
            spinlock_acquire(&state->lock);
            state->currentThread = nullptr;
            PickNext(false);
            spinlock_release(&state->lock);
        } else if (thread == state->idleThread) {
            spinlock_acquire(&state->lock);
            state->idleThread = nullptr;
            state->isIdle = 0; // should already be 0, but just to be safe
            spinlock_release(&state->lock);
        } else {
            bool found = false;
            
            // go forwards through nice levels
            for (int i = 0; i < NICE_LEVELS; i++) {
                ThreadList& list = state->threads[i];
                list.lock();
                if (list.getCount() == 0) {
                    list.unlock();
                    continue;
                }

                Thread* t = list.getHead();
                
                for (uint64_t i = 0; i < list.getCount(); i++) {
                    if (t == nullptr)
                        break;
                    if (t == thread) {
                        list.remove(t);
                        found = true;
                        break;
                    }
                    t = t->GetThreadListData().next;
                }

                list.unlock();

                if (found)
                    break;
            }

            return found;
        }
        return true;
    }

    bool RemoveThread(Thread* thread, ProcessorState* state) {
        if (state == nullptr) {
            if (state != GetCurrentProcessorState()) {
                if (state->processor == nullptr)
                    PANIC("ProcessorState doesn't have a Processor!");
                state->processor->Halt(true);
            }
            return RemoveThreadFromState(thread, state);
        } else {
            bool found = false;
            state = &g_BSPState;
            for (uint64_t i = 0; i < g_processorCount; i++) {
                if (state == nullptr)
                    break;

                found = RemoveThreadFromState(thread, state);
                if (found)
                    break;
                state = state->next;
            }
            return found;
        }
    }

    void TimerTick(uint64_t msSinceLast, void* data) {
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return;

        spinlock_acquire(&(state->lock));

        if (state->startAllowed == 0) {
            spinlock_release(&(state->lock));
            return;
        }

        int oldNice = -1;
        
        Thread* oldThread = state->currentThread;
        if (oldThread != nullptr) {
            oldThread->SetTimeRemaining(oldThread->GetTimeRemaining() - msSinceLast);
            if (oldThread->GetTimeRemaining() > 0) {
                SaveThreadFromINT(oldThread, data);
                spinlock_release(&(state->lock));
                return;
            }

            oldNice = oldThread->GetParent()->GetNice();

            ThreadList& list = state->threads[oldNice];
            list.lock();
            list.pushBack(oldThread);
            list.unlock();
            state->currentThread = nullptr;
        }

        PickNext(false);

        state->currentThread->SetTimeRemaining(DEFAULT_TIMESLICE);

        RunThreadOnINTReturn(state->currentThread, oldThread, data);

        spinlock_release(&(state->lock));
    }

    void PickNext(bool lockState) { // the actual algorithm
        ProcessorState* state = GetCurrentProcessorState();
        if (state == nullptr)
            return;

        Thread* thread = nullptr;
        int nice = -1;
        
        // go forwards through nice levels
        for (int i = 0; i < NICE_LEVELS; i++) {
            ThreadList& list = state->threads[i];
            list.lock();
            if (list.getCount() == 0 || (state->runCounts[i] == 2 && i != 0 && thread != nullptr)) {
                list.unlock();
                continue;
            }
            
            if (thread != nullptr)
                state->threads[nice].unlock();
            else if (state->runCounts[i] == 2)
                state->runCounts[i] = 0;
            thread = list.getHead();
            nice = i;
        }

        bool stolen = false;

        if (thread == nullptr) {
            thread = StealThreadFromOther(state, &nice);
            if (thread == nullptr)
                thread = state->idleThread;
            else
                stolen = true;
            if (thread == nullptr)
                PANIC("Scheduler: Nothing to run on current CPU");
        }
        
        state->runCounts[nice]++;
        if (!stolen) {
            state->threads[nice].popFront();
            state->threads[nice].unlock();
        }

        for (int i = nice + 1; i < NICE_LEVELS; i++) {
            if (state->runCounts[i] == 2)
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
                if (list.getCount() == 0 || (current->runCounts[i] == 2 && i != 0 && thread != nullptr)) {
                    list.unlock();
                    continue;
                }
                
                if (thread != nullptr)
                    state->threads[nice].unlock();
                else if (current->runCounts[i] == 2)
                    current->runCounts[i] = 0;
                thread = list.getHead();
                nice = i;
            }

            if (thread != nullptr) {
                if (niceOut != nullptr)
                    *niceOut = nice;
                state->threads[nice].popFront();
                state->threads[nice].unlock();
                return thread;
            }
        }
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
        ProcessorState* state = new ProcessorState;
        memset((void*)state, 0, sizeof(ProcessorState));
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

        RunThread(current->currentThread);
    }

    void WaitForStart(ProcessorState* state) {
        if (state == nullptr)
            state = GetCurrentProcessorState();

        while (state->startAllowed == 0) {
#ifdef __x86_64__
            __asm__ volatile ("pause" ::: "memory");
#endif
        }
    }

    bool isRunning() {
        return g_isRunning > 0;
    }

} // namespace Scheduler