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

#ifndef _x86_64_LAPIC_HPP
#define _x86_64_LAPIC_HPP

#include <stdint.h>

#include "../ISR.hpp"

enum class x86_64_LAPIC_Register {
    LAPIC_ID     = 0x020,
    LAPIC_VER    = 0x030,
    TPR          = 0x080,
    APR          = 0x090,
    PPR          = 0x0A0,
    EOI          = 0x0B0,
    RRD          = 0x0C0,
    LogicalDest  = 0x0D0,
    DestFormat   = 0x0E0,
    SpuriousIV   = 0x0F0,
    ISR0         = 0x100,
    ISR1         = 0x110,
    ISR2         = 0x120,
    ISR3         = 0x130,
    ISR4         = 0x140,
    ISR5         = 0x150,
    ISR6         = 0x160,
    ISR7         = 0x170,
    TMR0         = 0x180,
    TMR1         = 0x190,
    TMR2         = 0x1A0,
    TMR3         = 0x1B0,
    TMR4         = 0x1C0,
    TMR5         = 0x1D0,
    TMR6         = 0x1E0,
    TMR7         = 0x1F0,
    IRR0         = 0x200,
    IRR1         = 0x210,
    IRR2         = 0x220,
    IRR3         = 0x230,
    IRR4         = 0x240,
    IRR5         = 0x250,
    IRR6         = 0x260,
    IRR7         = 0x270,
    Error        = 0x280,
    LVT_CMCI     = 0x2F0,
    ICR0         = 0x300,
    ICR1         = 0x310,
    LVT_Timer    = 0x320,
    LVT_Thermal  = 0x330,
    LVT_PMC      = 0x340,
    LVT_LINT0    = 0x350,
    LVT_LINT1    = 0x360,
    LVT_ERR      = 0x370,
    InitialCount = 0x380,
    CurrentCount = 0x390,
    DivideConfig = 0x3E0
};


#define LAPIC_TIMER_PERIOD 1'000'000'000 // 1ms in picoseconds, 1kHz
#define LAPIC_TIMER_INT 0xFE

class x86_64_Processor;

class x86_64_LAPIC {
public:
    x86_64_LAPIC(bool BSP = false, uint8_t ID = 0xFF);

    void SetAddressOverride(uint64_t address); // must be called before Init

    void Init(bool started = false);
    bool InitTimer();

    void AddNMISource(uint8_t LINT, bool activeLow, bool levelTriggered); // must be called before Init

    void StartCPU();

    void WriteRegister(x86_64_LAPIC_Register reg, uint32_t value);
    uint32_t ReadRegister(x86_64_LAPIC_Register reg);

    void WriteRegister(uint64_t reg, uint32_t value);
    uint32_t ReadRegister(uint64_t reg);

    void SendEOI();

    uint8_t GetID() const;

    void SetTimerPeriod(uint64_t period); // in picoseconds

    void TimerInterrupt(x86_64_Processor* proc, x86_64_ISR_Frame* frame);

private:
    bool m_BSP;
    uint8_t m_ID; // LAPIC ID
    uint64_t m_LAPICBase;
    bool m_addressOverride;
    struct NMISource {
        bool used;
        bool activeLow;
        bool levelTriggered;
    } m_NMISources[2]; // only 2 LINTs
    uint64_t m_timerPeriod; // picoseconds
    uint32_t m_timerTicks;
    uint8_t m_timerDivisor;
};

extern x86_64_LAPIC* g_BSP_LAPIC;

#endif /* _x86_64_LAPIC_HPP */