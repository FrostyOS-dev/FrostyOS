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

#include "KernelSymbols.hpp"

extern "C" {

const void* _text_start_addr   = &__text_start;
const void* _text_end_addr     = &__text_end;
const void* _rodata_start_addr = &__rodata_start;
const void* _rodata_end_addr   = &__rodata_end;
const void* _data_start_addr   = &__data_start;
const void* _data_end_addr     = &__data_end;
const void* _bss_start_addr    = &__bss_start;
const void* _bss_end_addr      = &__bss_end;
const void* _kernel_end_addr   = &__kernel_end;
const void* _ctors_start_addr  = &__ctors_start;
const void* _ctors_end_addr    = &__ctors_end;
const void* _dtors_start_addr  = &__dtors_start;
const void* _dtors_end_addr    = &__dtors_end;

}
