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

#include "Thread.hpp"
#include "Process.hpp"

#include <string.h>

Thread::Thread() : m_EntryPoint({nullptr, nullptr}), m_Parent(nullptr), m_TID(UINT64_MAX), m_Stack(0), m_ThreadListData{nullptr, nullptr}, m_TimeRemaining(0) {

}

Thread::Thread(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) : m_EntryPoint(entryPoint), m_Parent(parent), m_TID(tid), m_Stack(0), m_ThreadListData{nullptr, nullptr}, m_TimeRemaining(0) {

}

Thread::~Thread() {

}

void Thread::Init(ThreadEntryPoint entryPoint, Process* parent, uint64_t tid) {
    m_EntryPoint = entryPoint;
    m_Parent = parent;
    m_TID = tid;
    m_Stack = 0;
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

void Thread::SetStack(uint64_t stack) {
    m_Stack = stack;
}

uint64_t Thread::GetStack() const {
    return m_Stack;
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
