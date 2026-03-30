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

#ifndef _SANITISERS_H
#define _SANITISERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SanitiserErrorData {
    const char* filename;
    uint32_t line;
	uint32_t column;
    const char* violation;
};

[[noreturn]] void SanitiserError(struct SanitiserErrorData* error);

#ifdef __cplusplus
}
#endif

#endif /* _SANITISERS_H */