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

#include "File.hpp"
#include "Process.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

typedef uint64_t (*systemCall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"

systemCall_t g_syscallTable[SYSTEM_CALL_COUNT] = {
#define ENUMERATE_CALL(u, l) (systemCall_t)&sys_##l,
    ENUMERATE_SYSTEM_CALLS(ENUMERATE_CALL)
#undef ENUMERATE_CALL
};

#pragma GCC diagnostic pop

uint64_t HandleSystemCall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    if (num >= SYSTEM_CALL_COUNT)
        return -ENOSYS;

    uint64_t rc = g_syscallTable[num](arg1, arg2, arg3, arg4, arg5);
    return rc;
}

bool UserRead(const void* userBuf, void* kBuf, size_t size, Process* currentProc) {
    if (userBuf == nullptr || kBuf == nullptr || size == 0 || currentProc == nullptr)
        return false;
    VMM::VMM* vmm = currentProc->GetVMM();
    if (!vmm->ValidateRead(userBuf, size))
        return false;
    memcpy(kBuf, userBuf, size);
    return true;
}

bool UserWrite(void* userBuf, const void* kBuf, size_t size, Process* currentProc, bool validate) {
    if (userBuf == nullptr || kBuf == nullptr || size == 0 || currentProc == nullptr)
        return false;
    VMM::VMM* vmm = currentProc->GetVMM();
    if (validate && !vmm->ValidateWrite(userBuf, size))
        return false;
    memcpy(userBuf, kBuf, size);
    return true;
}
