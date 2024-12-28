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

#include "Heap.hpp"
#include "PageManager.hpp"
#include "PagingUtil.hpp"
#include "PMM.hpp"
#include "arch/x86_64/Memory/PageTables.hpp"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <spinlock.h>
#include <math.h>
#include <util.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

Heap::Heap() : m_firstChunk(nullptr), m_freeMem(0), m_metadataMem(0), m_usedMem(0), m_totalMem(0), m_allocator(nullptr), m_deallocator(nullptr) {
    spinlock_init(&m_lock);
}

Heap::Heap(HeapAllocator_t allocator, HeapDeallocator_t deallocator) : m_firstChunk(nullptr), m_freeMem(0), m_metadataMem(0), m_usedMem(0), m_totalMem(0), m_allocator(allocator), m_deallocator(deallocator) {
    spinlock_init(&m_lock);
}

Heap::~Heap() {
    
}

void* Heap::allocate(size_t size) {
    size = ALIGN_UP(size, 16);
    Verify();
    spinlock_acquire(&m_lock);
    ChunkHeader* chunk = m_firstChunk;
    spinlock_release(&m_lock);

    ChunkHeader* prevChunk = nullptr;
    BlockHeader* lastBlock = nullptr;

    while (chunk != nullptr) {
        spinlock_acquire(&chunk->lock);
        BlockHeader* current = chunk->firstBlock;
        BlockHeader* prev = nullptr;
        if (current->inUse && current->size == 0) { // special case for when a chunk is fully allocated
            prevChunk = chunk;
            chunk = current->next.nextChunk;
            lastBlock = current;
            spinlock_release(&prevChunk->lock);
            continue;
        }
        do {
            assert(current != nullptr);

            size_t requiredSize;
            if (prev == nullptr && current->isLastBlock)
                requiredSize = size + sizeof(BlockHeader);
            else
                requiredSize = size;
            
            if (!current->inUse && current->size >= requiredSize) {
                BlockHeader* block = current;
                if (block->size > size + sizeof(BlockHeader)) {
                    // dbgprintf("allocate case 0\n");
                    BlockHeader* newBlock = (BlockHeader*)((uint64_t)block + size + sizeof(BlockHeader));
                    newBlock->next.nextBlock = block->next.nextBlock;
                    newBlock->size = block->size - size - sizeof(BlockHeader);
                    newBlock->inUse = false;
                    newBlock->isStartOfChunk = false;
                    newBlock->isEndOfChunk = block->isEndOfChunk;
                    newBlock->isLastBlock = block->isLastBlock;
                    block->next.nextBlock = nullptr;
                    block->size = size;
                    block->isEndOfChunk = false;
                    block->isLastBlock = false;
                    current = newBlock;

                    spinlock_acquire(&m_lock);
                    m_freeMem -= sizeof(BlockHeader);
                    m_metadataMem += sizeof(BlockHeader);
                    spinlock_release(&m_lock);

                    if (prev != nullptr)
                        prev->next.nextBlock = newBlock;
                    else
                        chunk->firstBlock = newBlock;
                }
                else {
                    if (prev != nullptr) {
                        prev->next.nextBlock = block->next.nextBlock;
                        if (block->isLastBlock)
                            prev->isLastBlock = true;
                    }
                    else if (!block->isLastBlock)
                        chunk->firstBlock = block->next.nextBlock;
                    else {
                        // dbgprintf("allocate case 1\n");
                        BlockHeader* newBlock = (BlockHeader*)((uint64_t)block + size + sizeof(BlockHeader));
                        newBlock->next.nextChunk = block->next.nextChunk;
                        newBlock->size = 0;
                        newBlock->inUse = true;
                        newBlock->isStartOfChunk = false;
                        newBlock->isEndOfChunk = block->isEndOfChunk;
                        newBlock->isLastBlock = true;
                        chunk->firstBlock = newBlock;
                        block->isEndOfChunk = false;
                        block->isLastBlock = false;
                        block->size -= sizeof(BlockHeader);
                        spinlock_acquire(&m_lock);
                        m_freeMem -= sizeof(BlockHeader);
                        m_metadataMem += sizeof(BlockHeader);
                        spinlock_release(&m_lock);
                    }
                }
                
                block->inUse = true;
                spinlock_acquire(&m_lock);
                m_freeMem -= block->size;
                m_usedMem += block->size;
                spinlock_release(&m_lock);
                spinlock_release(&chunk->lock);
                Verify();
                return (void*)((uint64_t)block + sizeof(BlockHeader));
            }
            if (current->isLastBlock)
                break;

            prev = current;
            current = current->next.nextBlock;
        } while (!prev->isLastBlock);
        prevChunk = chunk;
        chunk = current->next.nextChunk;
        lastBlock = current;
        spinlock_release(&prevChunk->lock);
    }

    assert(chunk == nullptr);
    assert(m_allocator != nullptr);

    size_t chunkSize = ALIGN_UP((size + sizeof(ChunkHeader) + 2 * sizeof(BlockHeader)), PAGE_SIZE);

    chunk = (ChunkHeader*)m_allocator(chunkSize);
    assert(chunk != nullptr);

    spinlock_init(&chunk->lock);

    BlockHeader* header = (BlockHeader*)((uint64_t)chunk + sizeof(ChunkHeader));

    chunk->size = chunkSize - sizeof(ChunkHeader);
    header->next.nextBlock = nullptr;
    header->size = chunkSize - sizeof(ChunkHeader) - 2 * sizeof(BlockHeader);
    header->inUse = false;
    header->isStartOfChunk = true;
    header->isEndOfChunk = false;
    header->isLastBlock = false;

    if (chunkSize > size + sizeof(ChunkHeader) + 2 * sizeof(BlockHeader)) {
        BlockHeader* newBlock = (BlockHeader*)((uint64_t)header + size + sizeof(BlockHeader));
        newBlock->next.nextBlock = nullptr;
        newBlock->size = chunkSize - size - sizeof(ChunkHeader) - 2 * sizeof(BlockHeader);
        newBlock->inUse = false;
        newBlock->isStartOfChunk = false;
        newBlock->isEndOfChunk = true;
        newBlock->isLastBlock = true;
        header->size = size;
        header->isEndOfChunk = false;
        header->isLastBlock = false;
        chunk->firstBlock = newBlock;
    }
    else {
        BlockHeader* newBlock = (BlockHeader*)((uint64_t)header + size + sizeof(BlockHeader));
        newBlock->next.nextBlock = nullptr;
        newBlock->size = 0;
        newBlock->inUse = true;
        newBlock->isStartOfChunk = false;
        newBlock->isEndOfChunk = true;
        newBlock->isLastBlock = true;
    }

    spinlock_acquire(&m_lock);
    if (m_firstChunk == nullptr)
        m_firstChunk = chunk;
    else {
        spinlock_acquire(&prevChunk->lock);
        lastBlock->next.nextChunk = chunk;
        spinlock_release(&prevChunk->lock);
    }
    m_totalMem += chunkSize;
    m_metadataMem += sizeof(ChunkHeader) + 2 * sizeof(BlockHeader);
    m_freeMem += chunkSize - header->size - 2 * sizeof(BlockHeader) - sizeof(ChunkHeader);
    m_usedMem += header->size;
    spinlock_release(&m_lock);
    // dbgprintf("chunkSize = %lu, size = %lu, header->size = %lu\n", chunkSize, size, header->size);
    Verify();
    return (void*)((uint64_t)header + sizeof(BlockHeader));
}

void Heap::free(void* ptr) {
    if (ptr == nullptr)
        return;
    Verify();
    spinlock_acquire(&m_lock);
    ChunkHeader* chunk = m_firstChunk;
    spinlock_release(&m_lock);

    BlockHeader* header = (BlockHeader*)((uint64_t)ptr - sizeof(BlockHeader));

    // dbgprintf("free: ptr = %lp, header = %lp, size = %lu\n", ptr, header, header->size);

    while (chunk != nullptr) {
        spinlock_acquire(&chunk->lock);
        BlockHeader* current = chunk->firstBlock;
        BlockHeader* prev = nullptr;
        BlockHeader* prevPrev = nullptr;
        do {
            assert(current != nullptr);
            
            if (((prev != nullptr && prev < header) || (void*)chunk < (void*)header) && header < current) {
                header->inUse = false;

                uint64_t size = header->size;

                if (prev != nullptr) {
                    if (!prev->inUse && (BlockHeader*)((uint64_t)prev + prev->size + sizeof(BlockHeader)) == header && !prev->isEndOfChunk) {
                        prev->next.nextBlock = current;
                        prev->size += header->size + sizeof(BlockHeader);
                        prev->isEndOfChunk = header->isEndOfChunk;
                        prev->isLastBlock = header->isLastBlock;
                        header = prev;
                        prev = prevPrev;
                        spinlock_acquire(&m_lock);
                        m_freeMem += sizeof(BlockHeader);
                        m_metadataMem -= sizeof(BlockHeader);
                        spinlock_release(&m_lock);
                    }
                    else
                        prev->next.nextBlock = header;
                }
                else
                    chunk->firstBlock = header;

                if ((BlockHeader*)((uint64_t)header + header->size + sizeof(BlockHeader)) == current && !header->isEndOfChunk) {
                    header->next.nextBlock = current->next.nextBlock;
                    header->size += current->size + sizeof(BlockHeader);
                    header->isEndOfChunk = current->isEndOfChunk;
                    header->isLastBlock = current->isLastBlock;
                    
                    if (prev != nullptr)
                        prev->next.nextBlock = header;
                    else if (!header->isLastBlock)
                        chunk->firstBlock = header;

                    current = header;

                    spinlock_acquire(&m_lock);
                    m_freeMem += sizeof(BlockHeader);
                    m_metadataMem -= sizeof(BlockHeader);
                    spinlock_release(&m_lock);
                }
                else {
                    header->next.nextBlock = current;
                    header->isLastBlock = false;
                }
                spinlock_acquire(&m_lock);
                m_freeMem += size;
                m_usedMem -= size;
                spinlock_release(&m_lock);

                // dbgprintf("free case 0\n");

                spinlock_release(&chunk->lock);
                Verify();
                RemoveEmptyChunks();
                Verify();
                return;
            }
            if (current->isLastBlock) {
                if (!current->isEndOfChunk && current < header && (BlockHeader*)((uint64_t)header + header->size + sizeof(BlockHeader)) <= (BlockHeader*)((uint64_t)chunk + chunk->size + sizeof(ChunkHeader))) {
                    if ((BlockHeader*)((uint64_t)current + current->size + sizeof(BlockHeader)) == header) {
                        size_t size = header->size;
                        current->size += header->size + sizeof(BlockHeader);
                        if (header->isEndOfChunk)
                            current->isEndOfChunk = true;
                        spinlock_release(&chunk->lock);
                        spinlock_acquire(&m_lock);
                        m_freeMem += sizeof(BlockHeader) + size;
                        m_usedMem -= size;
                        m_metadataMem -= sizeof(BlockHeader);
                        spinlock_release(&m_lock);
                        // dbgprintf("free case 1\n");
                        Verify();
                        RemoveEmptyChunks();
                        Verify();
                        return;
                    }
                    // dbgprintf("free case 2\n");
                    header->next.nextBlock = current->next.nextBlock;
                    header->isStartOfChunk = false;
                    header->isLastBlock = true;
                    current->next.nextBlock = header;
                    current->isLastBlock = false;
                    current->inUse = false;
                    spinlock_acquire(&m_lock);
                    m_freeMem += header->size;
                    m_usedMem -= header->size;
                    spinlock_release(&m_lock);
                    spinlock_release(&chunk->lock);
                    // dbgprintf("header = %lp, current = %lp, chunk = %lp\n", header, current, chunk);
                    Verify();
                    RemoveEmptyChunks();
                    Verify();
                    return;
                }
                else
                    break;
            }
            prevPrev = prev;
            prev = current;
            current = current->next.nextBlock;
        } while (!prev->isLastBlock);
        ChunkHeader* nextChunk = current->next.nextChunk;
        spinlock_release(&chunk->lock);
        chunk = nextChunk;
    }
    Verify();
}

void* Heap::reallocate(void* ptr, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }
    if (ptr == nullptr)
        return allocate(new_size);

    size_t old_size = ((BlockHeader*)((uint64_t)ptr - sizeof(BlockHeader)))->size;
    void* new_ptr = allocate(new_size);
    if (new_ptr == nullptr)
        return nullptr;
    memcpy(new_ptr, ptr, ulmin(old_size, new_size));
    free(ptr);
    return new_ptr;
}

void Heap::SetAllocator(HeapAllocator_t allocator, HeapDeallocator_t deallocator) {
    m_allocator = allocator;
    m_deallocator = deallocator;
}

void Heap::RemoveEmptyChunks() {
    spinlock_acquire(&m_lock);
    ChunkHeader* chunk = m_firstChunk;
    spinlock_release(&m_lock);
    ChunkHeader* prevChunk = nullptr;
    while (chunk != nullptr) {
        spinlock_acquire(&chunk->lock);
        BlockHeader* block = chunk->firstBlock;
        assert(block != nullptr);
        if (block->size + sizeof(BlockHeader) == chunk->size && block->isStartOfChunk && block->isEndOfChunk) {
            if (prevChunk != nullptr) {
                spinlock_acquire(&prevChunk->lock);
                BlockHeader* prevBlock = prevChunk->firstBlock;
                if (prevBlock != nullptr) {
                    
                    while (prevBlock != nullptr) {
                        if (prevBlock->isLastBlock)
                            break;
                        prevBlock = prevBlock->next.nextBlock;
                    }
                    prevBlock->next.nextChunk = block->next.nextChunk;
                }
                size_t size = chunk->size + sizeof(ChunkHeader);
                spinlock_acquire(&m_lock);
                m_totalMem -= chunk->size + sizeof(ChunkHeader);
                m_metadataMem -= sizeof(ChunkHeader) + sizeof(BlockHeader);
                m_freeMem -= chunk->size - sizeof(BlockHeader);
                spinlock_release(&chunk->lock);
                spinlock_release(&m_lock);
                m_deallocator(chunk, size);
                chunk = prevBlock->next.nextChunk;
                spinlock_release(&prevChunk->lock);
            }
            else {
                m_firstChunk = block->next.nextChunk;
                size_t size = chunk->size + sizeof(ChunkHeader);
                spinlock_acquire(&m_lock);
                m_totalMem -= size;
                m_metadataMem -= sizeof(ChunkHeader) + sizeof(BlockHeader);
                m_freeMem -= chunk->size - sizeof(BlockHeader);
                spinlock_release(&chunk->lock);
                m_deallocator(chunk, size);
                chunk = m_firstChunk;
                spinlock_release(&m_lock);
            }
            continue;
        }
        while (block != nullptr) {
            if (block->isLastBlock)
                break;
            block = block->next.nextBlock;
        }
        prevChunk = chunk;
        chunk = block->next.nextChunk;
        spinlock_release(&prevChunk->lock);
    }
}

void Heap::Print(fd_t fd) {
    ChunkHeader* chunk = m_firstChunk;
    while (chunk != nullptr) {
        fprintf(fd, "Chunk %lp, size = %lu\n", chunk, chunk->size);
        BlockHeader* block = chunk->firstBlock;
        if (block == nullptr)
            return;
        while (block != nullptr) {
            fprintf(fd, "Block %lp, size = %lu, inUse = %d, isStartOfChunk = %d, isEndOfChunk = %d, isLastBlock = %d\n", block, block->size, block->inUse, block->isStartOfChunk, block->isEndOfChunk, block->isLastBlock);
            if (block->isLastBlock)
                break;
            block = block->next.nextBlock;
        }
        chunk = block->next.nextChunk;
    }
}

void Heap::Verify() {
    assert((m_freeMem + m_usedMem + m_metadataMem) == m_totalMem);
    assert((int64_t)m_freeMem >= 0);
    assert((int64_t)m_usedMem >= 0);
    assert((int64_t)m_metadataMem >= 0);
    assert((int64_t)m_totalMem >= 0);
    uint64_t current_free = 0;
    ChunkHeader* chunk = m_firstChunk;
    while (chunk != nullptr) {
        assert ((chunk->size & (1UL << 63)) == 0);
        BlockHeader* block = chunk->firstBlock;
        BlockHeader* prev = nullptr;
        assert(block != nullptr && (uint64_t)block > (1UL << 48));
        while (block != nullptr) {
            if (block->size == 0 && block->inUse) {
                assert(block->isLastBlock);
            }
            assert(block >= (BlockHeader*)((uint64_t)chunk + sizeof(ChunkHeader)));
            if (prev != nullptr) {
                assert(((uint64_t)prev + sizeof(BlockHeader) + prev->size) <= (uint64_t)block);
                assert(prev->next.nextBlock == block);
            }
            if (!(((uint64_t)block + block->size + sizeof(BlockHeader)) <= ((uint64_t)chunk + chunk->size + sizeof(ChunkHeader)))) {
                dbgprintf("block = %lp, block->size = %lu, chunk = %lp, chunk->size = %lu\n", block, block->size, chunk, chunk->size);
                assert(false);
            }
            current_free += block->size;
            if (block->isEndOfChunk) {
                assert((((uint64_t)block + block->size + sizeof(BlockHeader)) % PAGE_SIZE) == 0);
            }
            if (block->isStartOfChunk) {
                assert((((uint64_t)block - sizeof(ChunkHeader)) % PAGE_SIZE) == 0);
            }
            if (block->isLastBlock)
                break;
            prev = block;
            block = block->next.nextBlock;
        }
        chunk = block->next.nextChunk;
    }
    if (current_free != m_freeMem) {
        dbgprintf("current_free = %lu, m_freeMem = %lu\n", current_free, m_freeMem);
        assert(false);
    }
    // else
    //     dbgprintf("m_freeMem = %lu\n", m_freeMem);
}

void Heap::VerifyBlock(ChunkHeader* chunk, BlockHeader* block) {
    assert(block >= (BlockHeader*)((uint64_t)chunk + sizeof(ChunkHeader)));
    assert(((uint64_t)block + block->size + sizeof(BlockHeader)) <= ((uint64_t)chunk + chunk->size + sizeof(ChunkHeader)));
    if (block->isEndOfChunk) {
        assert((((uint64_t)block + block->size + sizeof(BlockHeader)) % PAGE_SIZE) == 0);
    }
    if (block->isStartOfChunk) {
        assert((((uint64_t)block - sizeof(ChunkHeader)) % PAGE_SIZE) == 0);
    }
}

#pragma GCC diagnostic pop

Heap g_VMMHeap;

void* ExpandVMMHeap(size_t size) {
    return to_HHDM(g_PMM->AllocatePages(DIV_ROUNDUP(size, PAGE_SIZE)));
}

void ShrinkVMMHeap(void* ptr, size_t size) {
    g_PMM->FreePages(HHDM_to_phys(ptr), DIV_ROUNDUP(size, PAGE_SIZE));
}

void InitVMMHeap() {
    g_VMMHeap.SetAllocator(ExpandVMMHeap, ShrinkVMMHeap);
}

extern "C" void* kmalloc_vmm(size_t size) {
    return g_VMMHeap.allocate(size);
    // dbgprintf("kmalloc_vmm: size = %lu\n", size);
    // uint64_t pageCount = DIV_ROUNDUP(size, PAGE_SIZE);
    // void* ptr = to_HHDM(g_PMM->AllocatePages(pageCount + 2));
    // if (ptr == nullptr)
    //     return nullptr;
    // x86_64_UnmapPage(g_Level4Table, (uint64_t)ptr);
    // x86_64_UnmapPage(g_Level4Table, ((uint64_t)ptr + (pageCount + 1) * PAGE_SIZE));
    // return (void*)((uint64_t)ptr + PAGE_SIZE);
}

extern "C" void kfree_vmm(void* ptr) {
    g_VMMHeap.free(ptr);
}

extern "C" void* krealloc_vmm(void *ptr, size_t size) {
    assert(false);
    return g_VMMHeap.reallocate(ptr, size);
}

extern "C" void* kcalloc_vmm(size_t nmemb, size_t size) {
    void* ptr = kmalloc_vmm(nmemb * size);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, nmemb * size);
    return ptr;
}

Heap g_kernelHeap;

void* ExpandKernelHeap(size_t size) {
    return g_KPM->AllocatePages(DIV_ROUNDUP(size, PAGE_SIZE));
}

void ShrinkKernelHeap(void* ptr, size_t size) {
    g_KPM->FreePages(ptr, DIV_ROUNDUP(size, PAGE_SIZE));
}

void InitKernelHeap() {
    g_kernelHeap.SetAllocator(ExpandKernelHeap, ShrinkKernelHeap);
}

extern "C" void* kmalloc(size_t size) {
    return g_kernelHeap.allocate(size);
}

extern "C" void kfree(void* ptr) {
    g_kernelHeap.free(ptr);
}

extern "C" void* krealloc(void *ptr, size_t size) {
    return g_kernelHeap.reallocate(ptr, size);
}

extern "C" void* kcalloc(size_t nmemb, size_t size) {
    void* ptr = kmalloc(nmemb * size);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, nmemb * size);
    return ptr;
}


size_t g_eternalHeapSize = 0;
size_t g_eternalHeapUsed = 0;
void* g_eternalHeap = nullptr;
spinlock_new(g_eternalHeapLock);

void InitEternalHeap() {
    g_eternalHeapSize = 0;
    g_eternalHeapUsed = 0;
    g_eternalHeap = g_KPM->AllocatePages(MiB(2) / PAGE_SIZE);
    spinlock_init(&g_eternalHeapLock);
}

extern "C" void* kmalloc_eternal(size_t size) {
    size = ALIGN_UP(size, 16);
    spinlock_acquire(&g_eternalHeapLock);
    void* addr = g_eternalHeap;
    g_eternalHeap = (void*)((uint64_t)g_eternalHeap + size);
    g_eternalHeapUsed += size;
    spinlock_release(&g_eternalHeapLock);
    return addr;
}

extern "C" void* kcalloc_eternal(size_t nmemb, size_t size) {
    void* ptr = kmalloc_eternal(nmemb * size);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, nmemb * size);
    return ptr;
}
