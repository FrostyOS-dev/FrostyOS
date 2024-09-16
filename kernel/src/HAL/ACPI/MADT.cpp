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

// #define MADT_DEBUG

#ifdef MADT_DEBUG
#include <stdio.h>
#endif

#ifdef __x86_64__
#include <arch/x86_64/Processor.hpp>

#include <arch/x86_64/interrupts/APIC/LAPIC.hpp>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace ACPI {

    void InitMADT(MADT* madt) {
#ifdef __x86_64__
        uint8_t CurrentLAPICID = g_BSP_LAPIC->GetID();
#endif


        uint64_t EntryCount = (madt->Header.Length - sizeof(MADT)) / sizeof(MADTEntry);
        MADTEntry* Entry = (MADTEntry*)((uint64_t)madt + sizeof(MADT));
        for (uint64_t i = 0; i < EntryCount; i++) {
            switch (Entry->Type) {
                case 0: {
                    MADT_LocalAPIC* LocalAPIC = (MADT_LocalAPIC*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPIC: ProcessorID=%d, APICID=%d, Flags=%d\n", LocalAPIC->ProcessorID, LocalAPIC->APICID, LocalAPIC->Flags);
#endif
                    // Do something with LocalAPIC
                    if (LocalAPIC->Flags & 1) {
                        // Processor is enabled
#ifdef __x86_64__
                        if (CurrentLAPICID == LocalAPIC->APICID) {
                            // This is the BSP
                            g_BSP->InitLAPIC(g_BSP_LAPIC);
                        }
#endif
                    }
                    break;
                }
                case 1: {
                    MADT_IOAPIC* IOAPIC = (MADT_IOAPIC*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPIC: IOAPICID=%d, Address=%x, GlobalSystemInterruptBase=%d\n", IOAPIC->IOAPICID, IOAPIC->Address, IOAPIC->GlobalSystemInterruptBase);
#endif
                    // Do something with IOAPIC
                    break;
                }
                case 2: {
                    MADT_IOAPICInterruptSourceOverride* IOAPICInterruptSourceOverride = (MADT_IOAPICInterruptSourceOverride*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPICInterruptSourceOverride: Bus=%d, Source=%d, GlobalSystemInterrupt=%d, Flags=%d\n", IOAPICInterruptSourceOverride->Bus, IOAPICInterruptSourceOverride->Source, IOAPICInterruptSourceOverride->GlobalSystemInterrupt, IOAPICInterruptSourceOverride->Flags);
#endif
                    // Do something with IOAPICInterruptSourceOverride
                    break;
                }
                case 3: {
                    MADT_IOAPICNMI* IOAPICNMI = (MADT_IOAPICNMI*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("IOAPICNMI: NMISource=%d, Flags=%d, GSI=%d\n", IOAPICNMI->NMISource, IOAPICNMI->Flags, IOAPICNMI->GSI);
#endif
                    // Do something with IOAPICNMI
                    break;
                }
                case 4: {
                    MADT_LocalAPICNMI* LocalAPICNMI = (MADT_LocalAPICNMI*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPICNMI: ProcessorID=%d, Flags=%d, LINT=%d\n", LocalAPICNMI->ProcessorID, LocalAPICNMI->Flags, LocalAPICNMI->LINT);
#endif
                    // Do something with LocalAPICNMI
                    break;
                }
                case 5: {
                    MADT_LocalAPICAddressOverride* LocalAPICAddressOverride = (MADT_LocalAPICAddressOverride*)Entry;
#ifdef MADT_DEBUG
                    dbgprintf("LocalAPICAddressOverride: Address=%x\n", LocalAPICAddressOverride->Address);
#endif
                    // Do something with LocalAPICAddressOverride
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
    }

}

#pragma GCC diagnostic pop
