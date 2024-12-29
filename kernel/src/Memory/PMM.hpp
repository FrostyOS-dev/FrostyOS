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
#include <spinlock.h>

#include "MemoryMap.hpp"

class PMM {
public:
    PMM();
    ~PMM();

    void Init(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount);

    void* AllocatePage();
    void FreePage(void* page);

    void* AllocatePages(uint64_t pageCount);
    void FreePages(void* pages, uint64_t pageCount);

    uint64_t GetFreePageCount();

private:

    void Verify();

private:

    struct FreeListNode {
        uint64_t PageCount;
        FreeListNode* Next;
    };

    FreeListNode* m_FreeListStart;
    FreeListNode* m_FreeListEnd;
    uint64_t m_FreeListNodeCount;

    uint64_t m_FreePageCount;
    uint64_t m_usedPageCount;
    uint64_t m_totalPageCount;

    spinlock_t m_Lock;
};

extern PMM* g_PMM;

#endif /* _PMM_HPP */