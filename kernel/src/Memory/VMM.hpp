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
#include <stdio.h>

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

    struct FrameDesc {
        uint32_t refCount;
        uint32_t flags;
    };

    struct Page {
        AVLTree::Node node;
        uint64_t physAddr; // 0 when not assigned
        Protection protection; // highest protection this page is capable of
        bool isWired; // pageable
        uint16_t flags;

        static Page* Alloc();
        static void Free(Page* p);
    };

    enum class ObjectKind : uint8_t {
        Backed,
        Anon
    };

    struct MemoryObject {
        MemoryObject() = default;
        uint64_t size; // pages
        uint64_t refCount;
        ObjectKind kind;
        AVLTree::IntrusiveTree<Page, offsetof(Page, node)> pages; // keyed by offset in bytes
        DefaultPager* pager;
        void* pagerData;
        Mutex lock;
    };

    struct MapEntry {
        AVLTree::Node node;
        uint64_t startVirt;
        uint64_t endVirt;

        MemoryObject* object;
        uint64_t objectOffset; // bytes
        
        MemoryObject* anon;
        uint64_t anonOffset; // bytes

        struct {
            Protection protection;
            CacheType cacheType;
            bool user;
            bool needsCopy; // True if write fault should trigger an anonymous copy
            bool isPrivate;
            bool zero;
        } flags;
    };

    struct AllocFlags {
        Protection protection;
        CacheType cacheType;
        bool user;
        bool isPrivate;
        bool zero;
        bool allocPhys;
        bool addrIsHint; // if provided address is unavailable and not null, allocate at a different address
        bool replace; // Allow for this allocation to replace any exisiting allocation(s)
    };

    constexpr AllocFlags DEFAULT_KALLOC_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, false, true, true, false, false, false};
    constexpr AllocFlags DEFAULT_KALLOC_PHYS_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, false, true, true, true, false, false};
    constexpr AllocFlags DEFAULT_ALLOC_FLAGS = {Protection::READ_WRITE, CacheType::DEFAULT, true, true, true, false, false, false};


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

        // For both FreePages and RemapPages, specifying a count of 0 (full region), and multipleRegions = true is invalid
        bool FreePages(void* virtAddr, uint64_t count = 0, bool multipleRegions = false);
        bool RemapPages(void* virtAddr, uint64_t count = 0, Protection prot = Protection::READ_WRITE, bool user = false, CacheType cacheType = CacheType::DEFAULT, bool multipleRegions = false);
        bool MapPages(void* virtAddr, uint64_t count);

        bool HandlePageFault(PageFaultCode code, uint64_t virtAddr, bool* busError = nullptr);

        bool PrepareAccess(const void* addr, size_t size, bool write, bool user = true);
        bool ValidateRead(const void* addr, size_t size, bool user = true);
        bool ValidateWrite(const void* addr, size_t size, bool user = true);
        bool CopyStringFromUser(char* kernelDst, const void* userSrc, size_t maxSize, size_t* outSize, bool user = true);

        bool Fork(VMM* other); // this function does NOT perform cleanup of created regions on error

        PageMapper* GetPageMapper();
        VMRegionAllocator* GetAllocator();

        void DumpRegions(fd_t fd);

    private:
        MapEntry* SplitMapEntry(MapEntry* entry, uint64_t lowerPages, MapEntry* spare); // split a map entry so that entry has a page count of newPageCount, returns the new upper part
        bool Internal_FreePages(void* virtAddr, uint64_t totalCount, bool multipleRegions, bool lock); // lock controls whether the mapEntries should be locked, and if the vmRegionAllocator should be locked

        bool ValidateAccess(const void* addr, size_t size, Protection need, bool user);
        bool PageIsReady(uint64_t virt, bool write);

        bool ResolveNeedsCopy(MapEntry* e); // entries lock is held
        void ReleaseEntry(MapEntry* e, bool invalidate);

        // UVM fields
        PageMapper* m_pageMapper;
        VMRegionAllocator* m_vmRegionAllocator;
        AVLTree::IntrusiveTree<MapEntry, offsetof(MapEntry, node)> m_mapEntries;
        Mutex m_mapEntriesLock;
    };

    MemoryObject* CreateMemObj();
    void ReleaseMemoryObject(MemoryObject* obj);
    void RefMemObj(MemoryObject* obj);
    bool UnrefMemObj(MemoryObject* obj);
    bool MemObjIsShared(MemoryObject* obj);
    bool InitCaches();

    // to be implemented in arch-specific code
    extern VMM* g_KVMM;
    extern FrameDesc* g_frameTable; // indexed by page number

};

#endif /* _VMM_HPP */