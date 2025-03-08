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

#ifndef _THREAD_HPP
#define _THREAD_HPP

#include <stdint.h>

#include <HAL/HAL.hpp>

#include "ThreadList.hpp"

struct ThreadEntryPoint {
    void (*EntryPoint)(void*);
    void* Data;
};

class Process;

class Thread {
public:
    Thread();
    Thread(ThreadEntryPoint entryPoint, Process* parent = nullptr, uint64_t tid = UINT64_MAX);
    ~Thread();

    void Init(ThreadEntryPoint entryPoint, Process* parent = nullptr, uint64_t tid = UINT64_MAX);

    void SetEntryPoint(ThreadEntryPoint entryPoint);
    ThreadEntryPoint GetEntryPoint() const;

    void SetParent(Process* parent);
    Process* GetParent() const;

    void SetTID(uint64_t tid);
    uint64_t GetTID() const;

    void SetRegisters(CPU_Registers& registers);
    const CPU_Registers& GetRegisters() const;
    CPU_Registers& GetMutableRegisters();

    void SetStack(uint64_t stack);
    uint64_t GetStack() const;

    void SetThreadListData(ThreadListItemInternalData& data);
    ThreadListItemInternalData& GetThreadListData();

    void SetTimeRemaining(uint64_t timeRemaining);
    uint64_t GetTimeRemaining() const;

private:
    ThreadEntryPoint m_EntryPoint;
    Process* m_Parent;
    uint64_t m_TID;
    CPU_Registers m_Registers;
    uint64_t m_Stack;
    ThreadListItemInternalData m_ThreadListData;
    uint64_t m_TimeRemaining;
};

#endif /* _THREAD_HPP */