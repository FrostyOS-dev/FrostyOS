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

#ifndef _SYSTEM_CALL_HPP
#define _SYSTEM_CALL_HPP

#include <stdint.h>

uint64_t HandleSystemCall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#define ENUMERATE_SYSTEM_CALLS(SC) \
    SC(EXIT, exit)

enum SystemCalls : uint64_t {
#define ENUMERATE_CALL(u, l) SYS_##u,
    ENUMERATE_SYSTEM_CALLS(ENUMERATE_CALL)
#undef ENUMERATE_CALL
};

#define SYSTEM_CALL_COUNT 1

#endif /* _SYSTEM_CALL_HPP */