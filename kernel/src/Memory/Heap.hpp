/*
Copyright (©) 2025-2026  Frosty515

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

#ifndef _KERNEL_HEAP_HPP
#define _KERNEL_HEAP_HPP

#include <stddef.h>
#include <spinlock.h>

#define HEAP_MIN_BLOCK_SIZE 16

struct HeapSectionAllocator {
    void* (*Allocate)(size_t size);
    void (*Free)(void* ptr, size_t size);
};

class HeapAllocator {
public:
    HeapAllocator();
    HeapAllocator(HeapSectionAllocator* allocator);
    ~HeapAllocator();

    void* Allocate(size_t size);
    void Free(void* ptr);

    void SetSectionAllocator(HeapSectionAllocator* allocator);
    HeapSectionAllocator* GetSectionAllocator();

    size_t GetSize(void* ptr) const;

private:
    void Verify();

private:
    struct [[gnu::packed]] HeapBlock {
        size_t size : 63;
        bool free : 1;
        HeapBlock* next;
    };
    struct [[gnu::packed]] HeapSection {
        size_t size;
        HeapSection* next;
        HeapBlock* firstBlock;
    };

    HeapSection* m_Head;
    HeapSectionAllocator* m_SectionAllocator;
    spinlock_t m_Lock;
    size_t m_UsedMemory;
    size_t m_FreeMemory;
    size_t m_MetadataMemory;
    size_t m_TotalMemory;
};

#endif /* _KERNEL_HEAP_HPP */