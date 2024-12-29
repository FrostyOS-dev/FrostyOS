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
#include "PagingUtil.hpp"

#include <assert.h>
#include <util.h>

PMM* g_PMM = nullptr;

PMM::PMM() : m_FreeListStart(nullptr), m_FreeListEnd(nullptr), m_FreeListNodeCount(0), m_FreePageCount(0), m_usedPageCount(0), m_totalPageCount(0), m_Lock(SPINLOCK_DEFAULT_VALUE) {

}

PMM::~PMM() {
    
}

void PMM::Init(MemoryMapEntry** memoryMap, uint64_t memoryMapEntryCount) {
    for (uint64_t i = 0; i < memoryMapEntryCount; i++) {
        if (memoryMap[i]->Type == MEMORY_MAP_ENTRY_USABLE) {
            m_totalPageCount += memoryMap[i]->Length >> 12;
            m_FreePageCount += memoryMap[i]->Length >> 12;
            m_FreeListNodeCount++;
            // insert at end of list
            if (m_FreeListEnd == nullptr) {
                m_FreeListStart = (FreeListNode*)to_HHDM(memoryMap[i]->Base);
                m_FreeListStart->PageCount = memoryMap[i]->Length >> 12;
                m_FreeListStart->Next = nullptr;
                m_FreeListEnd = m_FreeListStart;
            }
            else {
                m_FreeListEnd->Next = (FreeListNode*)to_HHDM(memoryMap[i]->Base);
                m_FreeListEnd = m_FreeListEnd->Next;
                m_FreeListEnd->PageCount = memoryMap[i]->Length >> 12;
                m_FreeListEnd->Next = nullptr;
            }
        }
    }

    m_usedPageCount = 0;

    spinlock_init(&m_Lock);
}

void* PMM::AllocatePage() {
    spinlock_acquire(&m_Lock);
    assert(m_FreePageCount > 0);

    FreeListNode* node = m_FreeListStart;

    if (node->PageCount == 1) {
        m_FreeListStart = m_FreeListStart->Next;
        m_FreeListNodeCount--;
        if (node == m_FreeListEnd)
            m_FreeListEnd = nullptr;
    }
    else {
        m_FreeListStart = (FreeListNode*)((uint64_t)m_FreeListStart + PAGE_SIZE);
        m_FreeListStart->PageCount = node->PageCount - 1;
        m_FreeListStart->Next = node->Next;
        if (node == m_FreeListEnd)
            m_FreeListEnd = m_FreeListStart;
    }

    m_FreePageCount--;
    m_usedPageCount++;

    spinlock_release(&m_Lock);
    return (void*)from_HHDM(node);
}

void PMM::FreePage(void* page) {
    spinlock_acquire(&m_Lock);
    FreeListNode* node = (FreeListNode*)to_HHDM(page);
    
    FreeListNode* current = m_FreeListStart;
    FreeListNode* previous = nullptr;

    while (current != nullptr) {
        assert(previous < current);
        if (current > node) {
            if (previous == nullptr) {
                if ((void*)((uint64_t)node + PAGE_SIZE) == current) {
                    node->PageCount = current->PageCount + 1;
                    node->Next = current->Next;
                    if (m_FreeListEnd == current)
                        m_FreeListEnd = node;
                }
                else {
                    node->PageCount = 1;
                    node->Next = m_FreeListStart;
                    m_FreeListNodeCount++;
                }
                m_FreeListStart = node;
            }
            else {
                assert(previous < node);
                if ((void*)((uint64_t)previous + previous->PageCount * PAGE_SIZE) == node) {
                    previous->PageCount++;
                    node = previous;
                }
                else {
                    previous->Next = node;
                    m_FreeListNodeCount++;
                    node->PageCount = 1;
                }

                if ((void*)((uint64_t)node + node->PageCount * PAGE_SIZE) == current) {
                    node->PageCount += current->PageCount;
                    node->Next = current->Next;
                    m_FreeListNodeCount--;
                    if (m_FreeListEnd == current)
                        m_FreeListEnd = node;
                }
                else
                    node->Next = current;
            }
            m_FreePageCount++;
            m_usedPageCount--;
            spinlock_release(&m_Lock);
            return;
        }
        previous = current;
        current = current->Next;
    }

    // if we get here, we need to insert at the end
    if (m_FreeListEnd == nullptr) {
        m_FreeListStart = node;
        m_FreeListEnd = node;
        node->Next = nullptr;
    }
    else if ((void*)((uint64_t)m_FreeListEnd + m_FreeListEnd->PageCount * PAGE_SIZE) == node)
        m_FreeListEnd->PageCount++;
    else {
        m_FreeListEnd->Next = node;
        m_FreeListEnd = node;
        node->Next = nullptr;
        m_FreeListNodeCount++;
    }

    m_FreePageCount++;
    m_usedPageCount--;

    spinlock_release(&m_Lock);
}

void* PMM::AllocatePages(uint64_t pageCount) {
    if (pageCount == 1)
        return AllocatePage(); // much faster

    spinlock_acquire(&m_Lock);
    assert(m_FreePageCount > 0);

    FreeListNode* current = m_FreeListStart;
    FreeListNode* previous = nullptr;

    while (current != nullptr) {
        if (current->PageCount >= pageCount) {
            if (current->PageCount == pageCount) {
                if (previous == nullptr) {
                    m_FreeListStart = current->Next;
                    m_FreeListNodeCount--;
                    if (current == m_FreeListEnd)
                        m_FreeListEnd = nullptr;
                }
                else {
                    previous->Next = current->Next;
                    if (current == m_FreeListEnd)
                        m_FreeListEnd = previous;
                    m_FreeListNodeCount--;
                }
            }
            else {
                if (previous == nullptr) {
                    m_FreeListStart = (FreeListNode*)((uint64_t)current + PAGE_SIZE * pageCount);
                    m_FreeListStart->PageCount = current->PageCount - pageCount;
                    m_FreeListStart->Next = current->Next;
                    if (current == m_FreeListEnd)
                        m_FreeListEnd = m_FreeListStart;
                }
                else {
                    previous->Next = (FreeListNode*)((uint64_t)current + PAGE_SIZE * pageCount);
                    previous->Next->PageCount = current->PageCount - pageCount;
                    previous->Next->Next = current->Next;
                    if (current == m_FreeListEnd)
                        m_FreeListEnd = previous->Next;
                }
            }

            m_FreePageCount -= pageCount;
            m_usedPageCount += pageCount;

            spinlock_release(&m_Lock);
            return (void*)from_HHDM(current);
        }
        previous = current;
        current = current->Next;
    }

    // if we get here, we failed to allocate
    spinlock_release(&m_Lock);
    return nullptr;
}

void PMM::FreePages(void* pages, uint64_t pageCount) {
    spinlock_acquire(&m_Lock);
    FreeListNode* node = (FreeListNode*)to_HHDM(pages);
    
    FreeListNode* current = m_FreeListStart;
    FreeListNode* previous = nullptr;

    while (current != nullptr) {
        assert(previous < current);
        if (current > node) {
            if (previous == nullptr) {
                if ((void*)((uint64_t)node + PAGE_SIZE * pageCount) == current) {
                    node->PageCount = current->PageCount + pageCount;
                    node->Next = current->Next;
                    if (m_FreeListEnd == current)
                        m_FreeListEnd = node;
                }
                else {
                    node->PageCount = pageCount;
                    node->Next = m_FreeListStart;
                    m_FreeListNodeCount++;
                }
                m_FreeListStart = node;
            }
            else {
                assert(previous < node);
                if ((void*)((uint64_t)previous + previous->PageCount * PAGE_SIZE) == node) {
                    previous->PageCount += pageCount;
                    node = previous;
                }
                else {
                    previous->Next = node;
                    m_FreeListNodeCount++;
                    node->PageCount = pageCount;
                }

                if ((void*)((uint64_t)node + node->PageCount * PAGE_SIZE) == current) {
                    node->PageCount += current->PageCount;
                    node->Next = current->Next;
                    m_FreeListNodeCount--;
                    if (m_FreeListEnd == current)
                        m_FreeListEnd = node;
                }
                else
                    node->Next = current;
            }
            m_FreePageCount += pageCount;
            m_usedPageCount -= pageCount;
            spinlock_release(&m_Lock);
            return;
        }
        previous = current;
        current = current->Next;
    }

    // if we get here, we need to insert at the end
    if (m_FreeListEnd == nullptr) {
        m_FreeListStart = node;
        m_FreeListEnd = node;
        node->Next = nullptr;
    }
    else if ((void*)((uint64_t)m_FreeListEnd + m_FreeListEnd->PageCount * PAGE_SIZE) == node)
        m_FreeListEnd->PageCount += pageCount;
    else {
        m_FreeListEnd->Next = node;
        m_FreeListEnd = node;
        node->Next = nullptr;
        m_FreeListNodeCount++;
    }

    m_FreePageCount += pageCount;
    m_usedPageCount -= pageCount;

    spinlock_release(&m_Lock);
}

uint64_t PMM::GetFreePageCount() {
    return m_FreePageCount;
}

void PMM::Verify() {
    FreeListNode* current = m_FreeListStart;
    FreeListNode* previous = nullptr;
    uint64_t pageCount = 0;
    for (uint64_t i = 0; i < m_FreeListNodeCount; i++) {
        assert(current != nullptr);
        if (previous != nullptr)
            assert(previous + previous->PageCount <= current);
        pageCount += current->PageCount;
        previous = current;
        current = current->Next;
    }
    assert(current == nullptr);
    assert(pageCount == m_FreePageCount);
    assert(m_FreeListEnd == previous);
    assert(m_totalPageCount == m_FreePageCount + m_usedPageCount);
}
