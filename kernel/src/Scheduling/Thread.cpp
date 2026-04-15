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
#include <util.h>

#include <Memory/VMM.hpp>

Thread::Thread() : m_EntryPoint({nullptr, nullptr}), m_Parent(nullptr), m_TID(UINT64_MAX), m_Stack(0), m_KernelStack(0), m_ThreadListData{nullptr, nullptr}, m_TimeRemaining(0), m_CPUInfo(nullptr, SPINLOCK_DEFAULT_VALUE), m_deleteProp(false) {

}

Thread::Thread(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) : m_EntryPoint(entryPoint), m_Parent(parent), m_TID(tid), m_Stack(0), m_KernelStack(0), m_ThreadListData{nullptr, nullptr}, m_TimeRemaining(0), m_deleteProp(false) {

}

Thread::~Thread() {

}

bool Thread::Init() {
    if (m_Parent == nullptr)
        return false;

    return CreateStacks();
}

bool Thread::Init(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) {
    m_EntryPoint = entryPoint;
    m_Parent = parent;
    m_TID = tid;
    m_Stack = 0;
    return Init();
}

bool Thread::Delete() {
    if (m_Parent == nullptr)
        return false;

    VMM::VMM* vmm = m_Parent->GetVMM();
    if (vmm == nullptr)
        return false;

    if (!vmm->FreePages(reinterpret_cast<void*>(m_KernelStack - KERNEL_STACK_SIZE)))
        return false;

    if (m_Parent->GetMode() == ProcessMode::USER && !vmm->FreePages(reinterpret_cast<void*>(m_Stack - DEFAULT_USER_STACK_SIZE)))
        return false;

    m_Stack = 0;
    m_KernelStack = 0;
    return true;
}

bool Thread::ExitCurrentThread(bool deleteThis) {
    Processor::DisableInterrupts();
    Thread* thread = Scheduler::RemoveCurrentThread(true);
    if (thread == nullptr)
        return false;
    thread->m_deleteProp.deleteThis = deleteThis;
    Scheduler::ProcessorState* state = GetCurrentProcessorState();
    Processor::SwapStack(Thread_ExitHelper, thread, state->kernelStack);
    return false;
}

void Thread::SetEntryPoint(ThreadEntryPoint entryPoint) {
    m_EntryPoint = entryPoint;
}

ThreadEntryPoint Thread::GetEntryPoint() const {
    return m_EntryPoint;
}

void Thread::SetParent(Process* parent) {
    m_Parent = parent;
}

Process* Thread::GetParent() const {
    return m_Parent;
}

void Thread::SetTID(uint64_t tid) {
    m_TID = tid;
}

uint64_t Thread::GetTID() const {
    return m_TID;
}

void Thread::SetRegisters(CPU_Registers& registers) {
    memcpy(&m_Registers, &registers, sizeof(CPU_Registers));
}

const CPU_Registers& Thread::GetRegisters() const {
    return m_Registers;
}

CPU_Registers& Thread::GetMutableRegisters() {
    return m_Registers;
}

bool Thread::CreateStacks() {
    if (m_Parent == nullptr)
        return false;

    VMM::VMM* vmm = m_Parent->GetVMM();
    if (vmm == nullptr)
        return false;

    void* stack = vmm->AllocatePages(DIV_ROUNDUP(KERNEL_STACK_SIZE, PAGE_SIZE), VMM::Protection::READ_WRITE, true);
    if (stack == nullptr)
        return false;
    m_KernelStack = reinterpret_cast<uint64_t>(stack) + KERNEL_STACK_SIZE;

    if (m_Parent->GetMode() == ProcessMode::USER) {
        stack = vmm->AllocatePages(DIV_ROUNDUP(DEFAULT_USER_STACK_SIZE, PAGE_SIZE), VMM::Protection::READ_WRITE, false);
        if (stack == nullptr)
            return false;
        m_Stack = reinterpret_cast<uint64_t>(stack) + DEFAULT_USER_STACK_SIZE;
    } else
        m_Stack = m_KernelStack;

    return true;
}

void Thread::SetStack(uint64_t stack) {
    m_Stack = stack;
}

uint64_t Thread::GetStack() const {
    return m_Stack;
}

void Thread::SetKernelStack(uint64_t stack) {
    m_KernelStack = stack;
}

uint64_t Thread::GetKernelStack() const {
    return m_KernelStack;
}

void Thread::SetThreadListData(ThreadListItemInternalData& data) {
    m_ThreadListData = data;
}

ThreadListItemInternalData& Thread::GetThreadListData() {
    return m_ThreadListData;
}

void Thread::SetTimeRemaining(uint64_t timeRemaining) {
    m_TimeRemaining = timeRemaining;
}

uint64_t Thread::GetTimeRemaining() const {
    return m_TimeRemaining;
}

Thread::CPUInfo* Thread::GetCPUInfo() {
    return &m_CPUInfo;
}

void Thread::SetDeleteProp(bool deleteSelf) {
    m_deleteProp.deleteThis = deleteSelf;
}

bool Thread::ShouldDelete() const {
    return m_deleteProp.deleteThis;
}

[[noreturn]] void Thread_ExitHelper(void* data) {
    Thread* thread = static_cast<Thread*>(data);
    if (!Scheduler::DeleteThread(thread))
        PANIC("Failed to delete thread on exit!")

    Scheduler::Yield();
    PANIC("Scheduler::Yield() returned!")
}
