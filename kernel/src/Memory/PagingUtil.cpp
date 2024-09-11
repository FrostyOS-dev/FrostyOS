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

#include "PagingUtil.hpp"

uint64_t g_HHDMOffset = 0;

void SetHHDMOffset(uint64_t offset) {
    g_HHDMOffset = offset;
}

void* to_HHDM(void* addr) {
    return (void*)((uint64_t)addr + g_HHDMOffset);
}

uint64_t to_HHDM(uint64_t addr) {
    return addr + g_HHDMOffset;
}

void* HHDM_to_phys(void* addr) {
    return (void*)((uint64_t)addr - g_HHDMOffset);
}

uint64_t HHDM_to_phys(uint64_t addr) {
    return addr - g_HHDMOffset;
}
