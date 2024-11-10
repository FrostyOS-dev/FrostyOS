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

#ifndef _PMM_HPP
#define _PMM_HPP

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>

#include "MemoryMap.hpp"

class PMM {
public:
    PMM();

    void Initialise(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount);

    void* AllocatePage();
    void FreePage(void* page);

    void* AllocatePages(uint64_t pageCount);
    void FreePages(void* pages, uint64_t pageCount);

    void* Allocate32bitPage(); // allocate a page in the lower 4GiB

    void Verify();
private:

private:
    struct FreeListNode {
        size_t size;
        FreeListNode* next;
    };

    FreeListNode* m_freeListHead = nullptr;

    spinlock_new(m_Lock);

    uint64_t m_freeMem = 0;
    uint64_t m_usedMem = 0;
    uint64_t m_reservedMem = 0;
    uint64_t m_totalMem = 0;
};

extern PMM* g_PMM;

#endif /* _PMM_HPP */