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

#include "MADT.hpp"

#include <assert.h>
#include <stdint.h>

#ifdef MADT_DEBUG
#include <stdio.h>
#endif

#ifdef __x86_64__
#include <arch/x86_64/ArchDefs.h>
#include <arch/x86_64/Panic.hpp>

#include <arch/x86_64/interrupts/APIC/LocalAPIC.hpp>
#include <arch/x86_64/interrupts/APIC/IOAPIC.hpp>
#endif

#include <DataStructures/LinkedList.hpp>

#include <Memory/PagingUtil.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/acpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

#pragma GCC diagnostic pop


bool InitMADT() {
    using namespace LinkedList;

    uacpi_table table;
    uacpi_status rc = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table);
    if (uacpi_unlikely_error(rc))
        return false;

    acpi_madt* MADT = static_cast<acpi_madt*>(table.ptr);
    assert(MADT != nullptr);

#ifdef __x86_64__
    uint8_t CurrentLAPICID = x86_64_GetLAPICID();

    uint64_t LAPICBase = 0;
    SimpleLinkedList<acpi_madt_lapic> lapics;
    SimpleLinkedList<acpi_madt_ioapic> ioapics;
    SimpleLinkedList<acpi_madt_interrupt_source_override> ISOs;
    SimpleLinkedList<acpi_madt_nmi_source> NMIs;
    SimpleLinkedList<acpi_madt_lapic_nmi> LNMIs;
#endif

    uint64_t current_size = sizeof(acpi_madt);
    acpi_entry_hdr* entry = MADT->entries;
    while (current_size < MADT->hdr.length) {
        switch (entry->type) {
#ifdef __x86_64__
        case ACPI_MADT_ENTRY_TYPE_LAPIC: {
            acpi_madt_lapic* lapic = reinterpret_cast<acpi_madt_lapic*>(entry);
            if (lapic->flags & 1)
                lapics.insert(lapic);
#ifdef MADT_DEBUG
            dbgprintf("LAPIC: Processor ID = %hhu, APIC ID = %hhu, flags = %u\n", lapic->uid, lapic->id, lapic->flags);
#endif
            break;
        }
        case ACPI_MADT_ENTRY_TYPE_IOAPIC: {
            acpi_madt_ioapic* ioapic = reinterpret_cast<acpi_madt_ioapic*>(entry);
            ioapics.insert(ioapic);
#ifdef MADT_DEBUG
            dbgprintf("IOAPIC: ID = %hhu, address = %x, GSI base = %u\n", ioapic->id, ioapic->address, ioapic->gsi_base);
#endif
            break;
        }
        case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: {
            acpi_madt_interrupt_source_override* iso = reinterpret_cast<acpi_madt_interrupt_source_override*>(entry);
            ISOs.insert(iso);
#ifdef MADT_DEBUG
            dbgprintf("Interrupt Source Override: Bus = %hhu, Source = %hhu, GSI = %u, flags = %hu\n", iso->bus, iso->source, iso->gsi, iso->flags);
#endif
            break;
        }
        case ACPI_MADT_ENTRY_TYPE_NMI_SOURCE: {
            acpi_madt_nmi_source* nmi = reinterpret_cast<acpi_madt_nmi_source*>(entry);
            NMIs.insert(nmi);
#ifdef MADT_DEBUG
            dbgprintf("NMI Source: flags = %hu, GSI = %u\n", nmi->flags, nmi->gsi);
#endif
            break;
        }
        case ACPI_MADT_ENTRY_TYPE_LAPIC_NMI: {
            acpi_madt_lapic_nmi* lnmi = reinterpret_cast<acpi_madt_lapic_nmi*>(entry);
            LNMIs.insert(lnmi);
#ifdef MADT_DEBUG
            dbgprintf("LAPIC NMI: Processor ID: %hhu, flags = %hu, LINT = %hhu\n", lnmi->uid, lnmi->flags, lnmi->lint);
#endif
            break;
        }
        case ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE: {
            acpi_madt_lapic_address_override* addr = reinterpret_cast<acpi_madt_lapic_address_override*>(entry);
            LAPICBase = addr->address;
            break;
        }
#endif
        default:
#ifdef MADT_DEBUG
            dbgprintf("MADT entry: type = %lu\n", entry->type);
#endif
            break;
        }

        current_size += entry->length;
        entry = reinterpret_cast<acpi_entry_hdr*>(reinterpret_cast<uint64_t>(entry) + entry->length);
    }

#ifdef __x86_64__

    for (uint64_t i = 0; i < lapics.getCount(); i++) { // use loop instead of Enumerate due to too many local variable accesses
        acpi_madt_lapic* lapic_table = lapics.get(i);
        if (lapic_table == nullptr)
            continue;

        bool BSP = CurrentLAPICID == lapic_table->id;
        x86_64_LAPIC* lapic = new x86_64_LAPIC(BSP, lapic_table->id);

        if (LAPICBase != 0)
            lapic->SetAddressOverride(LAPICBase);

        struct Data {
            x86_64_LAPIC* lapic;
            acpi_madt_lapic* lapic_table;
        } d = {lapic, lapic_table};

        LNMIs.Enumerate([](acpi_madt_lapic_nmi* lnmi, void* data) -> bool {
            Data* d = static_cast<Data*>(data);
            if (lnmi->uid == 0xFF || lnmi->uid == d->lapic_table->uid)
                d->lapic->AddNMISource(lnmi->lint, (lnmi->flags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_LOW, (lnmi->flags & ACPI_MADT_TRIGGERING_MASK) == ACPI_MADT_TRIGGERING_LEVEL);
            return true;
        }, &d);

        lapic->Init(BSP);
    }

    if (ioapics.getCount() == 0)
        PANIC("I/O APIC support is required");

    ioapics.Enumerate([](acpi_madt_ioapic* ioapic_table, void* data) -> bool {
        x86_64_IOAPIC* ioapic = new x86_64_IOAPIC(to_HHDM(ioapic_table->address), ioapic_table->gsi_base);
        ioapic->Init();
        return true;
    }, nullptr);

    ISOs.Enumerate([](acpi_madt_interrupt_source_override* iso, void* data) -> bool {
        x86_64_IOAPIC_SetRedirection(iso->source, iso->gsi, (iso->flags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_LOW, (iso->flags & ACPI_MADT_TRIGGERING_MASK) == ACPI_MADT_TRIGGERING_LEVEL);
        return true;
    }, nullptr);

#endif

    return true;
}
