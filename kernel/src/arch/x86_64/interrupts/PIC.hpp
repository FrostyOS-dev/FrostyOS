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

#ifndef _x86_64_PIC_HPP
#define _x86_64_PIC_HPP

#include <stdint.h>

// remap the PICs and mask all IRQs
void x86_64_InitPIC();

void x86_64_PIC_SendEOI(uint8_t irq);

void x86_64_PIC_MaskIRQ(uint8_t irq);
void x86_64_PIC_UnmaskIRQ(uint8_t irq);

#endif /* _x86_64_PIC_HPP */