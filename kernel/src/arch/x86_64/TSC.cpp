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

#include "CPUID.h"
#include "Processor.hpp"
#include "TSC.hpp"

extern "C" void x86_64_TSCEnable();

bool x86_64_IsInvariantTSCSupported(x86_64_Processor* proc) {
    x86_64_CPUIDResult res = x86_64_CPUID(1, 0);
    if ((res.EDX & 0x10) == 0)
        return false;

    const x86_64_CPUInfo* info = proc->GetCPUInfo();
    if (info->maxExtendedCPUID < 0x80000007)
        return false;

    res = x86_64_CPUID(0x80000007, 0);
    return (res.EDX & 0x100) > 0;
}

uint64_t x86_64_TSCNativeGetFrequency(x86_64_Processor* proc) {
    const x86_64_CPUInfo* info = proc->GetCPUInfo();
    if (info->vendor != x86_VENDOR_INTEL || info->maxCPUID < 0x15)
        return 0;

    x86_64_CPUIDResult result = x86_64_CPUID(0x15, 0);
    if (result.EAX == 0 || result.EBX == 0)
        return 0;

    uint64_t crystalFreq = result.ECX;

    if (crystalFreq == 0 && info->maxCPUID >= 0x16) {
        x86_64_CPUIDResult result2 = x86_64_CPUID(0x16, 0);
        crystalFreq = result2.EAX * 1'000'000 * result.EAX / result.EBX;
    }

    if (crystalFreq == 0)
        return 0;

    x86_64_LAPIC* lapic = proc->GetLAPIC();
    if (lapic != nullptr)
        lapic->SetTimerPeriod(1'000'000'000'000 / crystalFreq); // set period in picoseconds. when the cystral frequency is known, it is also the lapic timer frequency


    return crystalFreq * result.EBX / result.EAX;
}

bool x86_64_TSCInit(x86_64_Processor* proc) {
    if (proc == nullptr) {
        proc = static_cast<x86_64_Processor*>(GetCurrentProcessor());
        if (proc == nullptr)
            return false;
    }

    if (!x86_64_IsInvariantTSCSupported(proc))
        return false;

    x86_64_TSCEnable();

    uint64_t freq = x86_64_TSCNativeGetFrequency(proc);
    if (freq == 0)
        return false;

    return true;
}
