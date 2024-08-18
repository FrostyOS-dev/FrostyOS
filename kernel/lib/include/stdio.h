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

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define stdin 0L
#define stdout 1L
#define stderr 2L
#define stddebug 3L

#define SEEK_SET 0
// FIXME: implement SEEK_CUR and SEEK_END

#define EOF (-1)

typedef long fd_t;

int getc();
int fgetc(const fd_t file);

void putc(const char c);
inline void putchar(const char c) { putc(c); }
void dbgputc(const char c);
void fputc(const fd_t file, const char c);

void puts(const char* str);
void dbgputs(const char* str);
void fputs(const fd_t file, const char* str);

int printf(const char* format, ...);
int vprintf(const char* format, va_list args);

int dbgprintf(const char* format, ...);
int dbgvprintf(const char* format, va_list args);

int fprintf(const fd_t file, const char* format, ...);
int vfprintf(const fd_t file, const char* format, va_list args);

int sprintf(char* buffer, const char* format, ...);
int vsprintf(char* buffer, const char* format, va_list args);

int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);

size_t fwrite(const void* ptr, const size_t size, const size_t count, const fd_t file);
size_t fread(void* ptr, size_t size, size_t count, const fd_t file);

fd_t fopen(const char* file, const char* mode);
int fclose(const fd_t file);

int fseek(const fd_t file, long int offset, int origin);

void rewind(const fd_t file);


// Non ISO C functions

size_t fgetsize(const fd_t file);

void stdio_force_unlock();

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */