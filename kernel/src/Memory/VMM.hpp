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

#ifndef _VMM_HPP
#define _VMM_HPP

#include <spinlock.h>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>

#include "Pager.hpp"

class PageMapper;
class VMRegionAllocator;

namespace VMM {

    struct PageFaultCode {
        bool present;
        bool write;
        bool user;
        bool execute;
    };

    enum class Protection : uint8_t {
        NONE = 0,
        READ = 1 << 0,
        WRITE = 1 << 1,
        EXECUTE = 1 << 2,
        READ_WRITE = READ | WRITE,
        READ_EXECUTE = READ | EXECUTE,
        READ_WRITE_EXECUTE = READ | WRITE | EXECUTE
    };

    enum class CacheType {
        UNCACHABLE,
        WRITE_BACK,
        WRITE_THROUGH,
        WRITE_PROTECTED,
        WRITE_COMBINING,
        DEFAULT = WRITE_BACK
    };

    struct Page {
        uint64_t physAddr; // 0 when not assigned
        Protection protection; // highest protection this page is capable of
        bool isWired; // pageable
    }; // doesn't need a lock 

    struct Anon {
        uint64_t refCount;
        uint64_t physAddr;
    };

    struct AnonMap {
        uint64_t refCount;
        size_t slotCount;
        spinlock_new(lock);
        Anon** slots; // pointer to an array of pointers to Anon objects
    };

    struct MemoryObject {
        uint64_t size;
        uint64_t refCount;

        AVLTree::wAVLTree<uint64_t, Page*> pages;

        DefaultPager* pager;
        void* pagerData;

        spinlock_new(lock);
    };

    struct MapEntry {
        uint64_t startVirt;
        uint64_t endVirt;
        uint64_t offset; // offset into memoryObject or anonMap

        AnonMap* anonMap;
        MemoryObject* memoryObject;

        struct {
            Protection protection;
            CacheType cacheType;
            bool user;
            bool needsCopy; // True if write fault should trigger an anonoymous copy
            bool isPrivate;
            bool zero;
        } flags;
        
        uint64_t wireCount; // currently unused
    };

    struct AllocFlags {
        Protection protection;
        CacheType cacheType;
        bool user;
        bool isPrivate;
        bool zero;
        bool allocPhys;
        bool addrIsHint; // if provided address is unavailable and not null, allocate at a different address
    };

    constexpr AllocFlags DEFAULT_KALLOC_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, false, true, true, false, false};
    constexpr AllocFlags DEFAULT_KALLOC_PHYS_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, false, true, true, true, false};
    constexpr AllocFlags DEFAULT_ALLOC_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, true, true, true, false, false};


    // The core VMM.
    // It uses the same region that its VMRegionAllocator uses.
    class VMM {
    public:
        VMM();
        VMM(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator);
        ~VMM();

        void Init(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator);
        void Delete(); // The PageMapper and VMRegionAllocator must be deleted separately.

        void* AllocateAnonPages(uint64_t count, AllocFlags flags = DEFAULT_KALLOC_FLAGS);
        void* AllocateAnonPages(uint64_t count, void* addr = nullptr, AllocFlags allocFlags = DEFAULT_KALLOC_FLAGS);

        void* AllocateBackedPages(uint64_t count, MemoryObject* obj, uint64_t offset = 0, void* addr = nullptr, AllocFlags allocFlags = DEFAULT_KALLOC_FLAGS); // offset will be aligned down to the nearest page
        
        // Allocate anonymous pages, but managed through a memory object. Intended for ram-based file system usage.
        void* AllocMemObjAnonPages(uint64_t count, void* pagerData, uint64_t offset = 0, AllocFlags flags = DEFAULT_KALLOC_FLAGS, MemoryObject** objOut = nullptr, DefaultPager* pager = g_defaultPager);

        bool FreePages(void* virtAddr, uint64_t count = 0);
        bool RemapPages(void* virtAddr, uint64_t count = 0, Protection prot = Protection::READ_WRITE, bool user = false, CacheType cacheType = CacheType::DEFAULT);
        bool MapPages(void* virtAddr, uint64_t count);

        bool HandlePageFault(PageFaultCode code, uint64_t virtAddr);

        bool ValidateRead(const void* addr, size_t size, bool user = true);
        bool ValidateWrite(const void* addr, size_t size, bool user = true);

        PageMapper* GetPageMapper();
        VMRegionAllocator* GetAllocator();

    private:
        MapEntry* SplitMapEntry(MapEntry* entry, uint64_t newPageCount); // split a map entry so that entry has a page count of newPageCount, returns the new upper part

        // UVM fields
        PageMapper* m_pageMapper;
        VMRegionAllocator* m_vmRegionAllocator;
        AVLTree::wAVLTree<uint64_t, MapEntry*> m_mapEntries; // Key is startVirt
    };

    extern VMM* g_KVMM; // to be implemented in arch-specific code

};

#endif /* _VMM_HPP */