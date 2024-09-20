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

#ifndef _HEAP_HPP
#define _HEAP_HPP

#include <stddef.h>
#include <stdio.h>
#include <spinlock.h>

struct BlockHeader;

struct ChunkHeader {
    size_t size;
    spinlock_t lock;
    BlockHeader* firstBlock;
    size_t _padding;
} __attribute__((packed));

struct BlockHeader {
    union {
        BlockHeader* nextBlock;
        ChunkHeader* nextChunk;
    } __attribute__((packed)) next;
    size_t size : 60;
    size_t inUse : 1;
    size_t isStartOfChunk : 1;
    size_t isEndOfChunk : 1;
    size_t isLastBlock : 1;
} __attribute__((packed));

typedef void* (*HeapAllocator_t)(size_t size);
typedef void (*HeapDeallocator_t)(void* ptr, size_t size);

class Heap {
public:
    Heap();
    Heap(HeapAllocator_t allocator, HeapDeallocator_t deallocator);
    ~Heap();

    void* allocate(size_t size);
    void free(void* ptr);
    void* reallocate(void* ptr, size_t new_size);

    void SetAllocator(HeapAllocator_t allocator, HeapDeallocator_t deallocator);

private:
    void RemoveEmptyChunks();

    void Print(fd_t fd);

    void Verify();
    void VerifyBlock(ChunkHeader* chunk, BlockHeader* block);

private:
    ChunkHeader* m_firstChunk;
    size_t m_freeMem;
    size_t m_metadataMem;
    size_t m_usedMem;
    size_t m_totalMem;

    spinlock_t m_lock;
    HeapAllocator_t m_allocator;
    HeapDeallocator_t m_deallocator;
};


void InitVMMHeap();

#endif /* _HEAP_HPP */