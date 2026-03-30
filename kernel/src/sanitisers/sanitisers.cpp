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

#include "sanitisers.h"

#include <stdio.h>
#include <string.h>

#include <HAL/HAL.hpp>

extern "C" [[noreturn]] void SanitiserError(SanitiserErrorData* error) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    snprintf(buffer, 1024, "Sanitiser error: %s at %s:%u:%u", error->violation, error->filename, error->line, error->column);
    PANIC(buffer);
}
