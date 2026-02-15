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

#ifndef _HAL_MCFG_HPP
#define _HAL_MCFG_HPP

#include <stdint.h>

bool InitMCFG();

bool MCFG_Validate(uint16_t segment, uint8_t bus);

bool MCFG_Read8(uint8_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
bool MCFG_Read16(uint16_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
bool MCFG_Read32(uint32_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);

bool MCFG_Write8(uint8_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
bool MCFG_Write16(uint16_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);
bool MCFG_Write32(uint32_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset);


#endif /* _HAL_MCFG_HPP */