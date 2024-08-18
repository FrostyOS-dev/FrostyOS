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

#ifndef _CSTR_H
#define _CSTR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

void* memset(void* dst, const uint8_t value, const size_t n);
void* memcpy(void* dst, const void* src, const size_t n);
void* memmove(void* dst, const void* src, const size_t n);
int memcmp(const void* s1, const void* s2, const size_t n);

size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);

char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);

int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);

char* strchr(const char* str, int character);

char* strrchr(const char* str, int character);

char* strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* _CSTR_H */
