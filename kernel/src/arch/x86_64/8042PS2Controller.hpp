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

#ifndef _X86_64_8042_PS2_CONTROLLER_HPP
#define _X86_64_8042_PS2_CONTROLLER_HPP

#include <stdint.h>

struct x86_64_8042_Status_Register {
    bool OutputBufferFull : 1;
    bool InputBufferFull : 1;
    bool SystemFlag : 1;
    bool CommandData : 1;
    bool Unknown1 : 1;
    bool Unknown2 : 1;
    bool TimeoutError : 1;
    bool ParityError : 1;
};

x86_64_8042_Status_Register x86_64_8042_ReadStatusRegister();
uint8_t x86_64_8042_ReadStatusRegister_Raw();

void x86_64_8042_WriteCommand(uint8_t command);

uint8_t x86_64_8042_ReadData();
void x86_64_8042_WriteData(uint8_t data);

#endif /* _X86_64_8042_PS2_CONTROLLER_HPP */