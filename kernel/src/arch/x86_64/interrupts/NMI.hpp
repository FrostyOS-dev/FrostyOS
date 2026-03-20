/*
Copyright (©) 2026  Frosty515

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

#ifndef _x86_64_NMI_HPP
#define _x86_64_NMI_HPP

#include <stdint.h>

#include <Scheduling/Scheduler.hpp>

#include "ISR.hpp"

enum class x86_64_NMIType {
    HALT,
    INVPAGES,
    YIELD,
    NONE
};

/*
Note:
Stopping a CPU with HALT and/or changing execution path with YIELD with the
intention of not returning to the original code is dangerous with the current setup.

This could result in a deadlock or worse.
*/

struct x86_64_NMI_InvPagesData {
    uint64_t start;
    uint64_t pageCount;
};

class x86_64_LAPIC;
class x86_64_Processor;

bool x86_64_HandleNMI(x86_64_ISR_Frame* frame);

namespace x86_64_GlobalNMI {
    void ForceAllowRaise();
    void SetData(x86_64_NMIType type, void* data = nullptr, uint64_t cpuCount = 0); // cpuCount of 0 for all
    void WaitForCPUs(uint64_t count = 0); // count = 0 for all
    void Raise(x86_64_LAPIC* lapic, x86_64_NMIType type, void* data = nullptr, uint64_t cpuCount = 0, bool wait = true); // cpuCount is that the number of CPUs to wait for, not the amount to send it to.
}

namespace x86_64_LocalNMI { // for NMIs on a specific CPU
    void Init(); // must be called on each CPU

    void Raise(x86_64_Processor* current, Scheduler::ProcessorState* target, x86_64_NMIType type, void* data = nullptr, bool wait = true);
    void Raise(x86_64_Processor* current, x86_64_Processor* target, x86_64_NMIType type, void* data = nullptr, bool wait = true);
}

#endif /* _x86_64_NMI_HPP */