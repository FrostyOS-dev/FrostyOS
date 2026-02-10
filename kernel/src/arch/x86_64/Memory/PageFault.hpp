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

#ifndef _x86_64_PAGE_FAULT_HPP
#define _x86_64_PAGE_FAULT_HPP

#include <stdint.h>

#include "../interrupts/ISR.hpp"

struct x86_64_PageFaultCode {
    bool present;
    bool write;
    bool user;
    bool reservedWrite;
    bool execute;
};

void x86_64_PageFaultHandler(x86_64_ISR_Frame* frame);

#endif /* _x86_64_PAGE_FAULT_HPP */