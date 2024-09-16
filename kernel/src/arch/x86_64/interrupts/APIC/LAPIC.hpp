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

#ifndef _x86_64_LOCAL_APIC_HPP
#define _x86_64_LOCAL_APIC_HPP

#include <stdint.h>

enum class x86_64_LAPIC_Register {
    LAPIC_ID            = 0x020, // Local APIC ID
    LAPIC_VERSION       = 0x030, // Local APIC Version
    TPR                 = 0x080, // Task Priority Register
    APR                 = 0x090, // Arbitration Priority Register
    PPR                 = 0x0A0, // Processor Priority Register
    EOI                 = 0x0B0, // End of Interrupt
    RRD                 = 0x0C0, // Remote Read Register
    LDR                 = 0x0D0, // Logical Destination Register
    DFR                 = 0x0E0, // Destination Format Register
    SVR                 = 0x0F0, // Spurious Interrupt Vector Register
    ISR                 = 0x100, // In-Service Register
    TMR                 = 0x180, // Trigger Mode Register
    IRR                 = 0x200, // Interrupt Request Register
    ERROR_STATUS        = 0x280, // Error Status Register
    CMCI_LVT            = 0x2F0, // Corrected Machine Check Interrupt LVT
    ICR                 = 0x300, // Interrupt Command Register
    LVT_TIMER           = 0x320, // Local Vector Timer
    LVT_THERMAL         = 0x330, // Local Vector Thermal Monitor
    LVT_PMC             = 0x340, // Local Vector Performance Monitor Counter
    LVT_LINT0           = 0x350, // Local Vector LINT0
    LVT_LINT1           = 0x360, // Local Vector LINT1
    LVT_ERROR           = 0x370, // Local Vector Error
    TIMER_INITIAL_COUNT = 0x380, // Timer Initial Count
    TIMER_CURRENT_COUNT = 0x390, // Timer Current Count
    TIMER_DIVIDE_CONFIG = 0x3E0, // Timer Divide Configuration
};

class x86_64_LAPIC {
public:
    x86_64_LAPIC(bool BSP = false);
    ~x86_64_LAPIC();

    void Init();

    void WriteRegister(x86_64_LAPIC_Register reg, uint32_t value);
    uint32_t ReadRegister(x86_64_LAPIC_Register reg);

    void WriteRegister(uint64_t reg, uint32_t value);
    uint32_t ReadRegister(uint64_t reg);

    uint8_t GetID();

private:
    bool m_BSP;
    uint64_t m_LAPICBase;
    uint8_t m_LAPICID;
};

extern x86_64_LAPIC* g_BSP_LAPIC;

#endif /* _x86_64_LOCAL_APIC_HPP */