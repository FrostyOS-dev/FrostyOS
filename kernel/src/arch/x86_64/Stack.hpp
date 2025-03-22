/*
Copyright (©) 2025  Frosty515

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

#ifndef _x86_64_STACK_HPP
#define _x86_64_STACK_HPP

#include <stdint.h>

struct x86_64_StackFrame {
    uint64_t RBP;
    uint64_t RIP;
};

void x86_64_WalkStackFrames(uint64_t RBP, void (*callback)(x86_64_StackFrame* frame));

#endif /* _x86_64_STACK_HPP */