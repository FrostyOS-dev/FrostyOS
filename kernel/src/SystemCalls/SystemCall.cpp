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

#include "Process.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stdint.h>

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

    return g_syscallTable[num](arg1, arg2, arg3, arg4, arg5);
}
