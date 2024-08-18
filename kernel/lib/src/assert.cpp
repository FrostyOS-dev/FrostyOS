/*
Copyright (©) 2022-2024  Frosty515

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

#include "assert.h"

#include <stdio.h>
#include <string.h>

extern __attribute__((noreturn)) void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function) {
    // FIXME: change this function call so it outputs to stderr once that works properly
    char buffer[1024];
    memset(buffer, 0, 1024);
    snprintf(buffer, 1023, "Assertion failed: \"%s\", file %s, line %u, function \"%s\"\n", assertion, file, line, function);
    dbgprintf(buffer);
    while (true) {}
}