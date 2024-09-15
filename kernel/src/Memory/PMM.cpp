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

#include "PMM.hpp"
#include "MemoryMap.hpp"
#include "PagingUtil.hpp"

#include <stddef.h>
#include <stdint.h>
#include <spinlock.h>
#include <util.h>

#include <HAL/HAL.hpp>

PMM* g_PMM = nullptr;

PMM::PMM() : m_freeListHead(nullptr), m_freeMem(0), m_usedMem(0), m_reservedMem(0) {
    spinlock_init(&m_Lock);
}

void PMM::Initialise(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount) {
    spinlock_acquire(&m_Lock);

    m_freeMem = 0;
    m_usedMem = 0;
    m_reservedMem = 0;

    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        MemoryMapEntry* entry = memoryMap[i];
        if (entry->type == MEMORY_MAP_USABLE)
            m_freeMem += entry->length;
        else
            m_reservedMem += entry->length;
    }

    MemoryMapEntry* first_free = nullptr;
    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        MemoryMapEntry* entry = memoryMap[i];
        if (entry->type == MEMORY_MAP_USABLE) {
            if (first_free == nullptr) {
                first_free = entry;
            }
            else {
                if (entry->base < first_free->base)
                    first_free = entry;
            }
        }
    }

    if (first_free == nullptr)
        PANIC("No usable memory found");

    m_freeListHead = (FreeListNode*)to_HHDM((void*)(first_free->base));
    m_freeListHead->size = first_free->length;
    m_freeListHead->next = nullptr;

    FreeListNode* current = m_freeListHead;

    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        MemoryMapEntry* entry = memoryMap[i];
        if (entry->type == MEMORY_MAP_USABLE && entry != first_free) {
            FreeListNode* node = (FreeListNode*)to_HHDM((void*)(entry->base));
            node->size = entry->length;
            node->next = nullptr;
            current->next = node;
            current = node;
        }
    }
    spinlock_release(&m_Lock);
}

void* PMM::AllocatePage() {
    spinlock_acquire(&m_Lock);
    FreeListNode* current = m_freeListHead;
    FreeListNode* previous = nullptr;
    while (current != nullptr) {
        if (current->size >= PAGE_SIZE) {
            void* page = HHDM_to_phys(current);
            if (current->size == PAGE_SIZE) {
                if (previous == nullptr)
                    m_freeListHead = current->next;
                else
                    previous->next = current->next;
            }
            else {
                FreeListNode* old = current;
                current = (FreeListNode*)((size_t)current + PAGE_SIZE);
                current->next = old->next;
                current->size = old->size - PAGE_SIZE;
                if (previous == nullptr)
                    m_freeListHead = current;
                else
                    previous->next = current;
            }
            m_freeMem -= PAGE_SIZE;
            m_usedMem += PAGE_SIZE;
            spinlock_release(&m_Lock);
            return page;
        }
        previous = current;
        current = current->next;
    }
    spinlock_release(&m_Lock);
    return nullptr;
}

void PMM::FreePage(void* page) {
    spinlock_acquire(&m_Lock);
    FreeListNode* current = m_freeListHead;
    FreeListNode* previous = nullptr;
    while (current != nullptr) {
        if (to_HHDM(page) > previous && to_HHDM(page) < current) {
            if (previous == nullptr) {
                FreeListNode* next = m_freeListHead;
                m_freeListHead = (FreeListNode*)to_HHDM(page);
                m_freeListHead->size = PAGE_SIZE;
                m_freeListHead->next = next;
            }
            else {
                if ((size_t)previous + previous->size == (size_t)to_HHDM(page))
                    previous->size += PAGE_SIZE;
                else if ((size_t)to_HHDM(page) + PAGE_SIZE == (size_t)current) {
                    current->size += PAGE_SIZE;
                    FreeListNode* old = current;
                    current = (FreeListNode*)to_HHDM(page);
                    current->size = old->size;
                    current->next = old->next;
                    previous->next = current;
                }
                else {
                    FreeListNode* node = (FreeListNode*)to_HHDM(page);
                    node->size = PAGE_SIZE;
                    node->next = current;
                    previous->next = node;
                }
            }
            m_freeMem += PAGE_SIZE;
            m_usedMem -= PAGE_SIZE;
            spinlock_release(&m_Lock);
            return;
        }
        previous = current;
        current = current->next;
    }
    spinlock_release(&m_Lock);
}

void* PMM::AllocatePages(uint64_t pageCount) {
    spinlock_acquire(&m_Lock);
    FreeListNode* current = m_freeListHead;
    FreeListNode* previous = nullptr;
    while (current != nullptr) {
        if (current->size >= PAGE_SIZE * pageCount) {
            void* page = HHDM_to_phys(current);
            if (current->size == PAGE_SIZE * pageCount) {
                if (previous == nullptr)
                    m_freeListHead = current->next;
                else
                    previous->next = current->next;
            }
            else {
                FreeListNode* old = current;
                current = (FreeListNode*)((size_t)current + PAGE_SIZE * pageCount);
                current->next = old->next;
                current->size = old->size - PAGE_SIZE * pageCount;
                if (previous == nullptr)
                    m_freeListHead = current;
                else
                    previous->next = current;
            }
            m_freeMem -= PAGE_SIZE * pageCount;
            m_usedMem += PAGE_SIZE * pageCount;
            spinlock_release(&m_Lock);
            return page;
        }
        previous = current;
        current = current->next;
    }
    spinlock_release(&m_Lock);
    return nullptr;
}

void PMM::FreePages(void* pages, uint64_t pageCount) {
    spinlock_acquire(&m_Lock);
    FreeListNode* current = m_freeListHead;
    FreeListNode* previous = nullptr;
    while (current != nullptr) {
        if (to_HHDM(pages) > previous && (void*)((uint64_t)to_HHDM(pages) + PAGE_SIZE * pageCount) <= current) {
            if (previous == nullptr) {
                FreeListNode* next = m_freeListHead;
                m_freeListHead = (FreeListNode*)to_HHDM(pages);
                m_freeListHead->size = PAGE_SIZE * pageCount;
                m_freeListHead->next = next;
            }
            else {
                if ((size_t)previous + previous->size == (size_t)to_HHDM(pages))
                    previous->size += PAGE_SIZE * pageCount;
                else if (((size_t)to_HHDM(pages) + PAGE_SIZE * pageCount) == (size_t)current) {
                    current->size += PAGE_SIZE * pageCount;
                    FreeListNode* old = current;
                    current = (FreeListNode*)to_HHDM(pages);
                    current->size = old->size;
                    current->next = old->next;
                    previous->next = current;
                }
                else {
                    FreeListNode* node = (FreeListNode*)to_HHDM(pages);
                    node->size = PAGE_SIZE * pageCount;
                    node->next = current;
                    previous->next = node;
                }
            }
            m_freeMem += PAGE_SIZE * pageCount;
            m_usedMem -= PAGE_SIZE * pageCount;
            spinlock_release(&m_Lock);
            return;
        }
        previous = current;
        current = current->next;
    }
    spinlock_release(&m_Lock);
}
