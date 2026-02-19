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

#include "Heap.hpp"
#include "PagingUtil.hpp"
#include "PMM.hpp"
#include "VMM.hpp"

#include <assert.h>
#include <stdio.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>
#include <util.h>

#include <HAL/HAL.hpp>

HeapAllocator::HeapAllocator() : m_Head(nullptr), m_SectionAllocator(nullptr), m_Lock(SPINLOCK_DEFAULT_VALUE), m_UsedMemory(0), m_FreeMemory(0), m_MetadataMemory(0), m_TotalMemory(0) {
}

HeapAllocator::HeapAllocator(HeapSectionAllocator* allocator) : m_Head(nullptr), m_SectionAllocator(allocator), m_Lock(SPINLOCK_DEFAULT_VALUE), m_UsedMemory(0), m_FreeMemory(0), m_MetadataMemory(0), m_TotalMemory(0) {
}

HeapAllocator::~HeapAllocator() {
}

void* HeapAllocator::Allocate(size_t size) {
    spinlock_acquire(&m_Lock);

    // go through all sections and find a free block
    HeapSection* section = m_Head;
    while (section != nullptr) {
        HeapBlock* previousBlock = nullptr;
        HeapBlock* block = section->firstBlock;
        while (block != nullptr) {
            if (block->free && block->size >= size) {
                if (block->size > size + sizeof(HeapBlock) + HEAP_MIN_BLOCK_SIZE) {
                    // split the block
                    HeapBlock* newBlock = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock) + size);
                    newBlock->size = block->size - size - sizeof(HeapBlock);
                    newBlock->free = true;
                    newBlock->next = block->next;
                    block->size = size;
                    block->next = newBlock;
                    m_MetadataMemory += sizeof(HeapBlock);
                    m_FreeMemory -= sizeof(HeapBlock);
                }
                block->free = false;
                // remove the block from the list
                if (previousBlock == nullptr)
                    section->firstBlock = block->next;
                else
                    previousBlock->next = block->next;

                block->next = nullptr;

                m_FreeMemory -= block->size;
                m_UsedMemory += block->size;

                spinlock_release(&m_Lock);
                return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock));
            }
            previousBlock = block;
            block = block->next;
        }
        section = section->next;
    }

    // no free block found, allocate a new section
    if (m_SectionAllocator == nullptr) {
        spinlock_release(&m_Lock);
        return nullptr;
    }

    if (m_SectionAllocator->Allocate == nullptr) {
        spinlock_release(&m_Lock);
        return nullptr;
    }

    size_t sectionSize = ALIGN_UP(sizeof(HeapSection) + sizeof(HeapBlock) + size, PAGE_SIZE);

    section = reinterpret_cast<HeapSection*>(m_SectionAllocator->Allocate(sectionSize));
    if (section == nullptr) {
        spinlock_release(&m_Lock);
        return nullptr;
    }

    m_TotalMemory += sectionSize;
    m_MetadataMemory += sizeof(HeapSection) + sizeof(HeapBlock);

    section->size = sectionSize - sizeof(HeapSection);
    section->next = m_Head;

    HeapBlock* block = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(section) + sizeof(HeapSection));
    block->size = section->size - sizeof(HeapBlock);
    block->free = false;
    block->next = nullptr;

    if (block->size > size + sizeof(HeapBlock) + HEAP_MIN_BLOCK_SIZE) {
        // split the block
        HeapBlock* newBlock = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock) + size);
        newBlock->size = block->size - size - sizeof(HeapBlock);
        newBlock->free = true;
        newBlock->next = nullptr;
        block->size = size;
        section->firstBlock = newBlock;
        m_MetadataMemory += sizeof(HeapBlock);
        m_FreeMemory += newBlock->size;
    }
    else
        section->firstBlock = nullptr;
    m_UsedMemory += block->size;


    m_Head = section;

    spinlock_release(&m_Lock);

    return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock));
}

void HeapAllocator::Free(void* ptr) {
    if (ptr == nullptr)
        return;

    spinlock_acquire(&m_Lock);

    HeapBlock* block = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(ptr) - sizeof(HeapBlock));
    assert(!block->free);
    block->free = true;

    // need to insert the block back into the list
    HeapSection* section = m_Head;
    while (section != nullptr) {
        if (reinterpret_cast<uint64_t>(block) >= reinterpret_cast<uint64_t>(section) + sizeof(HeapSection) && reinterpret_cast<uint64_t>(block) < reinterpret_cast<uint64_t>(section) + section->size) {
            HeapBlock* previousBlock = nullptr;
            HeapBlock* currentBlock = section->firstBlock;
            while (currentBlock != nullptr) {
                if (reinterpret_cast<uint64_t>(block) < reinterpret_cast<uint64_t>(currentBlock) && (previousBlock == nullptr || reinterpret_cast<uint64_t>(block) >= reinterpret_cast<uint64_t>(previousBlock) + sizeof(HeapBlock) + previousBlock->size)) {
                    if (previousBlock == nullptr) {
                        size_t size = block->size;
                        if (reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock) + block->size == reinterpret_cast<uint64_t>(currentBlock)) {
                            block->size += sizeof(HeapBlock) + currentBlock->size;
                            block->next = currentBlock->next;
                            m_MetadataMemory -= sizeof(HeapBlock);
                            m_FreeMemory += sizeof(HeapBlock);
                        }
                        else
                            block->next = currentBlock;
                        m_FreeMemory += size;
                        m_UsedMemory -= size;
                        block->free = true;
                        section->firstBlock = block;
                    }
                    else {
                        size_t size = block->size;
                        // merge with previous block
                        if (reinterpret_cast<uint64_t>(previousBlock) + sizeof(HeapBlock) + previousBlock->size == reinterpret_cast<uint64_t>(block)) {
                            previousBlock->size += sizeof(HeapBlock) + block->size;
                            block = previousBlock;
                            m_MetadataMemory -= sizeof(HeapBlock);
                            m_FreeMemory += sizeof(HeapBlock);
                        }
                        else {
                            block->free = true;
                            previousBlock->next = block;
                        }

                        // merge with current block
                        if (reinterpret_cast<uint64_t>(block) + sizeof(HeapBlock) + block->size == reinterpret_cast<uint64_t>(currentBlock)) {
                            block->size += sizeof(HeapBlock) + currentBlock->size;
                            block->next = currentBlock->next;
                            m_MetadataMemory -= sizeof(HeapBlock);
                            m_FreeMemory += sizeof(HeapBlock);
                        }
                        else
                            block->next = currentBlock;

                        m_FreeMemory += size;
                        m_UsedMemory -= size;
                    }
                    break;
                }
                previousBlock = currentBlock;
                currentBlock = currentBlock->next;
            }
            if (currentBlock == nullptr) {
                if (previousBlock == nullptr) {
                    block->next = nullptr;
                    section->firstBlock = block;
                    m_FreeMemory += block->size;
                    m_UsedMemory -= block->size;
                }
                else {
                    size_t size = block->size;
                    // merge with previous block
                    if (reinterpret_cast<uint64_t>(previousBlock) + sizeof(HeapBlock) + previousBlock->size == reinterpret_cast<uint64_t>(block)) {
                        previousBlock->size += sizeof(HeapBlock) + block->size;
                        block = previousBlock;
                        m_MetadataMemory -= sizeof(HeapBlock);
                        m_FreeMemory += sizeof(HeapBlock);
                    }
                    else {
                        block->free = true;
                        previousBlock->next = block;
                    }
                    block->next = nullptr;

                    m_FreeMemory += size;
                    m_UsedMemory -= size;
                }
            }
            break;
        }
        section = section->next;
    }

    // remove unused sections, these are sections that have 1 free block with a size (plus the header) that is the same as the section size
    HeapSection* previousSection = nullptr;
    section = m_Head;
    while (section != nullptr) {
        if (section->firstBlock != nullptr && section->firstBlock->next == nullptr && section->firstBlock->size + sizeof(HeapBlock) == section->size) {
            if (previousSection == nullptr)
                m_Head = section->next;
            else
                previousSection->next = section->next;

            m_TotalMemory -= section->size + sizeof(HeapSection);
            m_MetadataMemory -= sizeof(HeapSection) + sizeof(HeapBlock);
            m_FreeMemory -= section->size - sizeof(HeapBlock);

            HeapSection* nextSection = section->next;

            if (m_SectionAllocator != nullptr && m_SectionAllocator->Free != nullptr)
                m_SectionAllocator->Free(section, section->size + sizeof(HeapSection));

            section = nextSection;
        }
        else {
            previousSection = section;
            section = section->next;
        }
    }

    spinlock_release(&m_Lock);
}

size_t HeapAllocator::GetSize(void* ptr) const {
    if (ptr == nullptr)
        return 0;

    HeapBlock* block = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(ptr) - sizeof(HeapBlock));
    return block->size;
}

void HeapAllocator::Verify() {
    assert(m_UsedMemory + m_FreeMemory + m_MetadataMemory == m_TotalMemory);
    assert(static_cast<int64_t>(m_UsedMemory) >= 0);
    assert(static_cast<int64_t>(m_FreeMemory) >= 0);
    assert(static_cast<int64_t>(m_MetadataMemory) >= 0);
    assert(static_cast<int64_t>(m_TotalMemory) >= 0);
}


HeapSectionAllocator g_VMMHeapSectionAllocator = {[](size_t size) -> void* {
    return to_HHDM(g_PMM->AllocatePages(DIV_ROUNDUP(size, PAGE_SIZE)));
}, [](void* ptr, size_t size){
    g_PMM->FreePages(from_HHDM(ptr), DIV_ROUNDUP(size, PAGE_SIZE));
}};

HeapSectionAllocator g_KHeapSectionAllocator = {[](size_t size) -> void* {
    return VMM::g_KVMM->AllocatePages(DIV_ROUNDUP(size, PAGE_SIZE), VMM::Protection::READ_WRITE, true);
}, [](void* ptr, size_t) {
    if (!VMM::g_KVMM->FreePages(ptr)) {
        PANIC("Kernel heap failed to free section");
    }
}};

HeapAllocator g_VMMHeapAllocator(&g_VMMHeapSectionAllocator);
HeapAllocator g_KHeapAllocator(&g_KHeapSectionAllocator);

extern "C" void* kcalloc_vmm(size_t num, size_t size) {
    void* ptr = g_VMMHeapAllocator.Allocate(num * size);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, num * size);
    return ptr;
}

extern "C" void kfree_vmm(void* ptr) {
    g_VMMHeapAllocator.Free(ptr);
}

extern "C" void* kmalloc_vmm(size_t size) {
    return g_VMMHeapAllocator.Allocate(size);
}

extern "C" void* krealloc_vmm(void* ptr, size_t size) {
    assert(false);
    return nullptr;
}

extern "C" void* kcalloc(size_t num, size_t size) {
    void* ptr = g_KHeapAllocator.Allocate(num * size);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, num * size);
    return ptr;
}

extern "C" void kfree(void* ptr) {
    g_KHeapAllocator.Free(ptr);
}

extern "C" void* kmalloc(size_t size) {
    return g_KHeapAllocator.Allocate(size);
}

extern "C" void* krealloc(void* ptr, size_t size) {
    if (ptr == nullptr)
        return g_KHeapAllocator.Allocate(size);
    if (size == 0) {
        g_KHeapAllocator.Free(ptr);
        return nullptr;
    }

    size_t oldSize = g_KHeapAllocator.GetSize(ptr);
    if (oldSize == 0)
        return nullptr;

    if (oldSize >= size)
        return ptr;

    void* newBuf = g_KHeapAllocator.Allocate(size);
    if (newBuf == nullptr)
        return nullptr;

    memcpy(newBuf, ptr, oldSize);

    g_KHeapAllocator.Free(ptr);
    return newBuf;
}
