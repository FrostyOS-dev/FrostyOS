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

#ifdef __x86_64__
#include <arch/x86_64/GDT.hpp>
#include <arch/x86_64/interrupts/IDT.hpp>

#include <arch/x86_64/Memory/PagingInit.hpp>

void HAL_EarlyInit(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount, void* fb_base, uint64_t fb_size, uint64_t kernel_virtual, uint64_t kernel_physical) {
    x86_64_GDTInit(g_GDT);
    x86_64_IDTInit();
    x86_64_InitPaging(memoryMap, memoryMapEntryCount, fb_base, fb_size, kernel_virtual, kernel_physical);
}

#endif