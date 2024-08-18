/*
Copyright (©) 2022-2023  Frosty515

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

#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
#define assert(expr) (void)(expr)
#else

    #define __ASSERT_FUNCTION __extension__ __PRETTY_FUNCTION__

    extern __attribute__((noreturn)) void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function);

    #if defined __cplusplus
        #define assert(expr) (static_cast<bool>(expr)	? void (0) : __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION))
    #else
        #define assert(expr)((void) sizeof ((expr) ? 1 : 0), __extension__ ({ if (expr); else __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION); }))
    #endif
#endif

#endif /* _ASSERT_H */