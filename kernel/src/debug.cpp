/*
Copyright (©) 2024  Frosty515

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

#include "debug.h"

#ifdef __x86_64__
#include <arch/x86_64/E9.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int64_t debug_putc(const char c) {
#ifdef __x86_64__
    return x86_64_debug_putc(c);
#else
    return -1;
#endif
}

int64_t debug_puts(const char* str) {
#ifdef __x86_64__
    return x86_64_debug_puts(str);
#else
    return -1;
#endif
}

#ifdef __cplusplus
}
#endif
