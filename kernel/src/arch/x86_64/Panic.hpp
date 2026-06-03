/*
Copyright (©) 2024-2026  Frosty515

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

extern "C" [[noreturn]] void x86_64_Panic(const char* message, void* registers, bool type = false); // type = false for normal panic, true for interrupt panic

extern "C" [[noreturn]] void x86_64_PrePanic();

#define PANIC(reason) do { g_x86_64_PanicReason = (reason); x86_64_PrePanic(); __builtin_unreachable(); } while (0)

#endif /* _PANIC_HPP */