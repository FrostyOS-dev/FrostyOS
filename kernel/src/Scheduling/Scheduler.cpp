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

#include "Scheduler.hpp"

#include <spinlock.h>
#include <string.h>

#include <DataStructures/LinkedList.hpp>

#include <Memory/PagingUtil.hpp>

#include <HAL/Processor.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Memory/PagingInit.hpp>

#include <arch/x86_64/Scheduling/Task.h>
#include <arch/x86_64/Scheduling/TaskUtil.hpp>
#endif

namespace Scheduler {

    ProcessorState g_BSPState;
    LinkedList::LockableLinkedList<Process> g_Processes(true);
    uint64_t g_LastPID = 0;
    spinlock_t g_PIDLock;

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
    }

    ProcessorState* GetProcessor(uint64_t id) {
        ProcessorState* head = &g_BSPState;
        while (head != nullptr) {
            if (head->id == id)
                return head;
            head = head->next;
        }
        return nullptr;
    }

    void RemoveProcessor(uint64_t id) {
        ProcessorState* head = &g_BSPState;
        while (head != nullptr) {
            if (head->id == id) {
                if (head->prev != nullptr)
                    head->prev->next = head->next;
                if (head->next != nullptr)
                    head->next->prev = head->prev;
                return;
            }
            head = head->next;
        }
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

    void ScheduleThread(Thread* thread) {
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

        ThreadList& list = g_BSPState.threads[nice];
        list.lock();
        list.pushBack(thread);
        list.unlock();
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

        if (thread == nullptr) {
            // TODO: idle, work stealing
            PANIC("Scheduler: Nothing to run on current CPU");
        }
        
        state->runCounts[nice]++;
        state->threads[nice].popFront();
        state->threads[nice].unlock();

        for (int i = nice + 1; i < NICE_LEVELS; i++) {
            if (state->runCounts[i] == 2)
                state->runCounts[i] = 0;
        }

        if (lockState)
            spinlock_acquire(&(state->lock));

        state->currentThread = thread;

        if (lockState)
            spinlock_release(&(state->lock));
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

    [[noreturn]] void Start() {
        PickNext();
        ProcessorState* state = GetCurrentProcessorState();
        assert(state != nullptr);

        state->startAllowed = 1;

        RunThread(state->currentThread);
    }

} // namespace Scheduler