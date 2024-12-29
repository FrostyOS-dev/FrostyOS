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

#ifndef _MEMORY_MAP_HPP
#define _MEMORY_MAP_HPP

#include <stdint.h>

#define MEMORY_MAP_ENTRY_USABLE                 0
#define MEMORY_MAP_ENTRY_RESERVED               1
#define MEMORY_MAP_ENTRY_ACPI_RECLAIMABLE       2
#define MEMORY_MAP_ENTRY_ACPI_NVS               3
#define MEMORY_MAP_ENTRY_BAD                    4
#define MEMORY_MAP_ENTRY_BOOTLOADER_RECLAIMABLE 5
#define MEMORY_MAP_ENTRY_KERNEL_AND_MODULES     6
#define MEMORY_MAP_ENTRY_FRAMEBUFFER            7

struct MemoryMapEntry {
    uint64_t Base;
    uint64_t Length;
    uint64_t Type;
};

#endif /* _MEMORY_MAP_HPP */