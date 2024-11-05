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

#include "PageManager.hpp"
#include "PageTable.hpp"
#include "PMM.hpp"
#include "VirtualMemoryAllocator.hpp"
#include "VMRegion.hpp"

#include <spinlock.h>
#include <stdlib.h>
#include <util.h>

PageManager::PageManager() : m_table(nullptr), m_vma(nullptr), m_user(false), m_lock(SPINLOCK_DEFAULT_VALUE), m_regions(true) {

}

PageManager::PageManager(PageTable* table, VirtualMemoryAllocator* vma, bool user) : m_table(table), m_vma(vma), m_user(user), m_lock(SPINLOCK_DEFAULT_VALUE), m_regions(true) {

}

PageManager::~PageManager() {

}

void PageManager::Initialise(PageTable* table, VirtualMemoryAllocator* vma, bool user) {
    m_table = table;
    m_vma = vma;
    m_user = user;
    spinlock_init(&m_lock);
}

uint64_t g_i;

void* PageManager::AllocatePages(uint64_t pageCount, PagePermission permission, PageCache cache) {
    void* address = m_vma->AllocatePages(pageCount);
    if (address == nullptr)
        return nullptr;
    VMRegion* region = (VMRegion*)kcalloc_vmm(1, sizeof(VMRegion));
    *region = VMRegion(address, (void*)((uint64_t)address + pageCount * PAGE_SIZE), { permission, cache, true, true, false, 0 });
    m_regions.insert(address, region);
    for (g_i = 0; g_i < pageCount; g_i++)
        m_table->Map((void*)((uint64_t)address + g_i * PAGE_SIZE), g_PMM->AllocatePage(), permission, cache, false);
    m_table->Flush(address, pageCount);
    return address;
}

void* PageManager::AllocatePage(PagePermission permission, PageCache cache) {
    return AllocatePages(1, permission, cache);
}

void PageManager::FreePages(void* address, uint64_t pageCount) {
    // for now, assume direct match
    VMRegion* region = m_regions.find(address);
    if (region == nullptr || (pageCount > 0 && region->GetSize() != pageCount * PAGE_SIZE)) {
        assert(false);
        return;
    }
    if (pageCount == 0)
        pageCount = region->GetSize() / PAGE_SIZE;
    for (g_i = 0; g_i < pageCount; g_i++) {
        void* phys_addr = m_table->GetPhysicalAddress((void*)((uint64_t)address + g_i * PAGE_SIZE));
        // m_table->Unmap((void*)((uint64_t)address + g_i * PAGE_SIZE), false);
        g_PMM->FreePage(phys_addr);
    }
    m_vma->FreePages(address, pageCount);
    m_regions.remove(address);
    kfree_vmm(region);
    m_table->Flush(address, pageCount);
}

void PageManager::FreePage(void* address) {
    return FreePages(address, 1);
}

void* PageManager::AllocatePages(void* address, uint64_t pageCount, PagePermission permission, PageCache cache) {
    void* addr = m_vma->AllocatePages(address, pageCount);
    if (addr == nullptr)
        return nullptr;
    VMRegion* region = (VMRegion*)kcalloc_vmm(1, sizeof(VMRegion));
    *region = VMRegion(addr, (void*)((uint64_t)addr + pageCount * PAGE_SIZE), { permission, cache, true, true, false, 0 });
    m_regions.insert(addr, region);
    for (uint64_t i = 0; i < pageCount; i++)
        m_table->Map((void*)((uint64_t)addr + i * PAGE_SIZE), g_PMM->AllocatePage(), permission, cache, false);
    m_table->Flush(addr, pageCount);
    return addr;
}

void* PageManager::AllocatePage(void* address, PagePermission permission, PageCache cache) {
    return AllocatePages(address, 1, permission, cache);
}

void* PageManager::MapPages(void* virtualAddress, void* physicalAddress, uint64_t pageCount, PagePermission permission, PageCache cache) {
    void* addr = nullptr;
    if (virtualAddress == nullptr)
        addr = m_vma->AllocatePages(pageCount);
    else
        addr = m_vma->AllocatePages(virtualAddress, pageCount);
    if (addr == nullptr)
        return nullptr;
    VMRegion* region = (VMRegion*)kcalloc_vmm(1, sizeof(VMRegion));
    *region = VMRegion(addr, (void*)((uint64_t)addr + pageCount * PAGE_SIZE), { permission, cache, false, true, false, 0 }); // assume non-pageable
    m_regions.insert(addr, region);
    for (uint64_t i = 0; i < pageCount; i++)
        m_table->Map((void*)((uint64_t)addr + i * PAGE_SIZE), (void*)((uint64_t)physicalAddress + i * PAGE_SIZE), permission, cache, false);
    m_table->Flush(addr, pageCount);
    return addr;
}

void* PageManager::MapPage(void* virtualAddress, void* physicalAddress, PagePermission permission, PageCache cache) {
    return MapPages(virtualAddress, physicalAddress, 1, permission, cache);
}

void PageManager::UnmapPages(void* virtualAddress, uint64_t pageCount) {
    VMRegion* region = m_regions.find(virtualAddress);
    if (region == nullptr || (pageCount > 0 && region->GetSize() != pageCount * PAGE_SIZE))
        return;
    if (pageCount == 0)
        pageCount = region->GetSize() / PAGE_SIZE;
    for (uint64_t i = 0; i < pageCount; i++)
        m_table->Unmap((void*)((uint64_t)virtualAddress + i * PAGE_SIZE), false);
    m_vma->FreePages(virtualAddress, pageCount);
    m_regions.remove(virtualAddress);
    kfree_vmm(region);
    m_table->Flush(virtualAddress, pageCount);
}

void PageManager::UnmapPage(void* virtualAddress) {
    return UnmapPages(virtualAddress, 1);
}

void PageManager::RemapPages(void* virtualAddress, PagePermission permission, PageCache cache) {
    VMRegion* region = m_regions.find(virtualAddress);
    if (region == nullptr)
        return;
    VMRegionFlags flags = region->GetFlags();
    flags.Permission = permission;
    flags.Cache = cache;
    region->SetFlags(flags);
    uint64_t pageCount = region->GetSize() / PAGE_SIZE;
    for (uint64_t i = 0; i < pageCount; i++)
        m_table->Remap((void*)((uint64_t)virtualAddress + i * PAGE_SIZE), permission, cache, false);
    m_table->Flush(virtualAddress, pageCount);
}

void PageManager::PrintRegions() {
    m_regions.Enumerate([](void* key, VMRegion* value, void*) {
        dbgprintf("Region: %p - %p\n", key, value->GetEnd());
    }, nullptr);
}

void PageManager::Verify() {

}

PageManager* g_KPM = nullptr;
