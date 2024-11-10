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

#include "MADT.hpp"
#include "arch/x86_64/ArchDefs.h"

#define MADT_DEBUG

#ifdef MADT_DEBUG
#include <stdio.h>
#endif

#include <Data-structures/LinkedList.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Processor.hpp>

#include <arch/x86_64/interrupts/APIC/LAPIC.hpp>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace ACPI {

    void InitMADT(MADT* madt) {
#ifdef __x86_64__
        uint8_t CurrentLAPICID = x86_64_GetLAPICID();
#endif
        LinkedList::SimpleLinkedList<MADT_LocalAPIC> LAPICs;
        LinkedList::SimpleLinkedList<MADT_IOAPIC> IOAPICs;
        LinkedList::SimpleLinkedList<MADT_IOAPICInterruptSourceOverride> IOAPICInterruptSourceOverrides;
        LinkedList::SimpleLinkedList<MADT_IOAPICNMI> IOAPICNMIs;
        LinkedList::SimpleLinkedList<MADT_LocalAPICNMI> LocalAPICNMIs;
        void* LAPICAddressOverride = nullptr;

        MADTEntry* Entry = (MADTEntry*)((uint64_t)madt + sizeof(MADT));
        while (Entry < (MADTEntry*)((uint64_t)madt + madt->Header.Length)) {
            switch (Entry->Type) {
                case 0: {
                    MADT_LocalAPIC* LocalAPIC = (MADT_LocalAPIC*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPIC: ProcessorID=%d, APICID=%d, Flags=%d\n", LocalAPIC->ProcessorID, LocalAPIC->APICID, LocalAPIC->Flags);
#endif
                    // Do something with LocalAPIC
                    if (LocalAPIC->Flags & 1) {
                        // Processor is enabled
                        LAPICs.insert(LocalAPIC);
                    }
                    break;
                }
                case 1: {
                    MADT_IOAPIC* IOAPIC = (MADT_IOAPIC*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPIC: IOAPICID=%d, Address=%x, GlobalSystemInterruptBase=%d\n", IOAPIC->IOAPICID, IOAPIC->Address, IOAPIC->GlobalSystemInterruptBase);
#endif
                    IOAPICs.insert(IOAPIC);
                    break;
                }
                case 2: {
                    MADT_IOAPICInterruptSourceOverride* IOAPICInterruptSourceOverride = (MADT_IOAPICInterruptSourceOverride*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPICInterruptSourceOverride: Bus=%d, Source=%d, GlobalSystemInterrupt=%d, Flags=%d\n", IOAPICInterruptSourceOverride->Bus, IOAPICInterruptSourceOverride->Source, IOAPICInterruptSourceOverride->GlobalSystemInterrupt, IOAPICInterruptSourceOverride->Flags);
#endif
                    IOAPICInterruptSourceOverrides.insert(IOAPICInterruptSourceOverride);
                    break;
                }
                case 3: {
                    MADT_IOAPICNMI* IOAPICNMI = (MADT_IOAPICNMI*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPICNMI: NMISource=%d, Flags=%d, GSI=%d\n", IOAPICNMI->NMISource, IOAPICNMI->Flags, IOAPICNMI->GSI);
#endif
                    IOAPICNMIs.insert(IOAPICNMI);
                    break;
                }
                case 4: {
                    MADT_LocalAPICNMI* LocalAPICNMI = (MADT_LocalAPICNMI*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPICNMI: ProcessorID=%d, Flags=%d, LINT=%d\n", LocalAPICNMI->ProcessorID, LocalAPICNMI->Flags, LocalAPICNMI->LINT);
#endif
                    LocalAPICNMIs.insert(LocalAPICNMI);
                    break;
                }
                case 5: {
                    MADT_LocalAPICAddressOverride* LocalAPICAddressOverride = (MADT_LocalAPICAddressOverride*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPICAddressOverride: Address=%x\n", LocalAPICAddressOverride->Address);
#endif
                    LAPICAddressOverride = (void*)LocalAPICAddressOverride->Address;
                    break;
                }
                case 9: {
                    MADT_Localx2APIC* Localx2APIC = (MADT_Localx2APIC*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("Localx2APIC: x2APICID=%d, Flags=%d, ProcessorUID=%d\n", Localx2APIC->x2APICID, Localx2APIC->Flags, Localx2APIC->ProcessorUID);
#endif
                    // Do something with Localx2APIC
                    break;
                }
            }
            Entry = (MADTEntry*)((uint64_t)Entry + Entry->Length);
        }

#ifdef __x86_64__
        for (uint64_t i = 0; i < LAPICs.getCount(); i++) {
            MADT_LocalAPIC* lapic_table = LAPICs.get(i);
            if (lapic_table == nullptr)
                continue;
            bool BSP = CurrentLAPICID == lapic_table->APICID;
            x86_64_LAPIC* lapic = new x86_64_LAPIC(BSP, lapic_table->APICID);

            if (LAPICAddressOverride != nullptr)
                lapic->SetAddressOverride(LAPICAddressOverride);

            for (uint64_t j = 0; j < LocalAPICNMIs.getCount(); j++) {
                MADT_LocalAPICNMI* nmi = LocalAPICNMIs.get(j);
                if (nmi == nullptr)
                    continue;
                if (nmi->ProcessorID == lapic_table->ProcessorID || nmi->ProcessorID == 0xFF)
                    lapic->AddNMISource(nmi->LINT, nmi->Flags & 1, nmi->Flags & 2);
            }

            lapic->Init(BSP);
        }

        for (uint64_t i = 0; i < IOAPICs.getCount(); i++) {
            MADT_IOAPIC* ioapic_table = IOAPICs.get(i);
            if (ioapic_table == nullptr)
                continue;
            // Do something with IOAPIC
        }
#endif
    }
}

#pragma GCC diagnostic pop
