/*
Copyright (©) 2026  Frosty515

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

#include "MCFG.hpp"

#include "../drivers/PCI.hpp"

#include <assert.h>
#include <stdint.h>
#include <util.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/PagingUtil.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/acpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

#pragma GCC diagnostic pop

AVLTree::wAVLTree<uint32_t, acpi_mcfg_allocation*> g_MCFGAllocations;

#define SEG_BUS_TO_KEY(seg, bus) (((uint32_t)seg << 16) | bus)
#define SEG_FROM_KEY(key) (key >> 16)
#define BUS_FROM_KEY(key) (key & 0xFF)

bool InitMCFG() {
    uacpi_table table;
    uacpi_status rc = uacpi_table_find_by_signature(ACPI_MCFG_SIGNATURE, &table);
    if (uacpi_unlikely_error(rc))
        return false;

    acpi_mcfg* MCFG = static_cast<acpi_mcfg*>(table.ptr);
    assert(MCFG != nullptr);

    uint64_t current_size = sizeof(acpi_mcfg);
    acpi_mcfg_allocation* entry = MCFG->entries;
    while (current_size < MCFG->hdr.length) {
        uint64_t phys = ALIGN_DOWN(entry->address, PAGE_SIZE);
        uint64_t mapSize = (entry->address % PAGE_SIZE) + (4096 * 8 * 32 * (entry->end_bus - entry->start_bus + 1));
        g_KPageMapper->MapPages(to_HHDM(phys), phys, DIV_ROUNDUP(mapSize, PAGE_SIZE), VMM::Protection::READ_WRITE, false, VMM::CacheType::UNCACHABLE);

        g_MCFGAllocations.lock();
        g_MCFGAllocations.Insert(SEG_BUS_TO_KEY(entry->segment, entry->start_bus), entry);
        g_MCFGAllocations.unlock();

        current_size += sizeof(acpi_mcfg_allocation);
        entry = reinterpret_cast<acpi_mcfg_allocation*>(reinterpret_cast<uint64_t>(entry) + sizeof(acpi_mcfg_allocation));
    }

    PCI_Read8 = &MCFG_Read8;
    PCI_Read16 = &MCFG_Read16;
    PCI_Read32 = &MCFG_Read32;
    PCI_Write8 = &MCFG_Write8;
    PCI_Write16 = &MCFG_Write16;
    PCI_Write32 = &MCFG_Write32;
    g_PCIAccessMethod = PCIAccessMethod::MCFG;

    return true;
}

acpi_mcfg_allocation* GetMCFGAlloc(uint16_t segment, uint8_t bus) {
    g_MCFGAllocations.lock();
    AVLTree::wAVLTreeNode* node = g_MCFGAllocations.FindNodeOrLower(SEG_BUS_TO_KEY(segment, bus));
    bool valid = node != nullptr && SEG_FROM_KEY(node->key) == segment;
    g_MCFGAllocations.unlock();
    if (!valid)
        return nullptr;
    
    acpi_mcfg_allocation* alloc = reinterpret_cast<acpi_mcfg_allocation*>(node->value);
    return bus >= alloc->start_bus && bus <= alloc->end_bus ? alloc : nullptr;
}

bool MCFG_Validate(uint16_t segment, uint8_t bus) {
    return GetMCFGAlloc(segment, bus) != nullptr;
}

bool MCFG_Read8(uint8_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read8(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Read16(uint16_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read16(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Read32(uint32_t* out, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read32(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Write8(uint8_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write8(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

bool MCFG_Write16(uint16_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write16(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

bool MCFG_Write32(uint32_t data, uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = GetMCFGAlloc(segment, bus);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write32(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

bool MCFG_Read8(uint8_t* out, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read8(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Read16(uint16_t* out, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read16(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Read32(uint32_t* out, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    if (out == nullptr)
        return false;

    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;
    
    uint64_t base = to_HHDM(alloc->address);
    *out = volatile_addr_read32(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset));
    return true;
}

bool MCFG_Write8(uint8_t data, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;
    
    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write8(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

bool MCFG_Write16(uint16_t data, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;

    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write16(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

bool MCFG_Write32(uint32_t data, void* allocPtr, uint8_t bus, uint8_t device, uint8_t function, uint32_t offset) {
    acpi_mcfg_allocation* alloc = static_cast<acpi_mcfg_allocation*>(allocPtr);
    if (alloc == nullptr)
        return false;
    
    uint64_t base = to_HHDM(alloc->address);
    volatile_addr_write32(base + (((bus - alloc->start_bus) << 20) | (device << 15) | (function << 12) | offset), data);
    return true;
}

void MCFG_EnumerateBusses(FunctionRef<bool(uint16_t segment, uint8_t bus, void* alloc)> callback) {
    g_MCFGAllocations.lock();
    g_MCFGAllocations.forEach([&](uint32_t key, acpi_mcfg_allocation* alloc) -> bool {
        for (int bus = alloc->start_bus; bus <= alloc->end_bus; bus++) {
            if (!callback(SEG_FROM_KEY(key), bus, alloc))
                return false;
        }
        return true;
    });
    g_MCFGAllocations.unlock();
}
