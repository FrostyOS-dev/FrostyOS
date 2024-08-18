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

#include "E9.h"
#include "IO.h"

#include <stddef.h>

int64_t x86_64_debug_putc(const char c) {
    x86_64_outb(0xe9, c);
    return 1;
}

int64_t x86_64_debug_puts(const char* str) {
    size_t i;
    for (i = 0; str[i] != '\0'; i++)
        x86_64_debug_putc(str[i]);
    return i;
}
