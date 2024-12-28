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

#ifndef _PANIC_HPP
#define _PANIC_HPP

extern char const* g_x86_64_PanicReason;

extern "C" [[noreturn]] void x86_64_Panic(const char* message);

extern "C" [[noreturn]] void x86_64_PrePanic();

#define PANIC(reason) {__asm__ volatile ("movq %1, %0" : "=m" (g_x86_64_PanicReason) : "p" (reason)); __asm__ volatile ("call x86_64_PrePanic"); __builtin_unreachable();}

#endif /* _PANIC_HPP */