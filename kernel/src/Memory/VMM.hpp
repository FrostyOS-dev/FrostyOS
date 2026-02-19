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

    struct Page {
        uint64_t physAddr; // 0 when not assigned
        Protection protection; // highest protection this page is capable of
        bool isWired; // pageable
        Page* next;
    }; // doesn't need a lock 

    struct MemoryObject {
        uint64_t size; // in bytes
        Page* pages;   // linked list of pages
        DefaultPager* pager;
        spinlock_new(lock);
    };

    struct MapEntry {
        uint64_t startVirt;
        uint64_t endVirt;
        MemoryObject* memoryObject;
        Protection protection;
        uint64_t memUsagePattern; // TODO
        uint64_t wireCount;
    };


    // The core VMM.
    // It uses the same region that its VMRegionAllocator uses.
    class VMM {
    public:
        VMM();
        VMM(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator);
        ~VMM();

        void Init(PageMapper* pageMapper, VMRegionAllocator* vmRegionAllocator);

        void* AllocatePages(uint64_t count, Protection prot = Protection::READ_WRITE, bool allocPhys = false);
        bool FreePages(void* virtAddr);

        bool MapMemory(uint64_t virtAddr, MemoryObject* memObj, Protection prot);
        bool UnmapMemory(uint64_t virtAddr);
        bool RemapMemory(uint64_t virtAddr, Protection prot);

        bool HandlePageFault(PageFaultCode code, uint64_t virtAddr);

    private:
        // UVM fields
        PageMapper* m_pageMapper;
        VMRegionAllocator* m_vmRegionAllocator;
        AVLTree::wAVLTree<uint64_t, MapEntry*> m_mapEntries; // Key is startVirt
    };

    extern VMM* g_KVMM; // to be implemented in arch-specific code

};

#endif /* _VMM_HPP */