/*
Copyright (©) 2024-2026  Frosty515

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

#ifndef _x86_64_ARCH_DEFINTIONS_H
#define _x86_64_ARCH_DEFINTIONS_H

#include <stdint.h>

#define PAUSE() __asm__ volatile ("pause" ::: "memory")

struct [[gnu::packed, gnu::aligned(8)]] x86_64_Registers {
    uint64_t RAX;
    uint64_t RBX;
    uint64_t RCX;
    uint64_t RDX;
    uint64_t RSI;
    uint64_t RDI;
    uint64_t RSP;
    uint64_t RBP;
    uint64_t R8;
    uint64_t R9;
    uint64_t R10;
    uint64_t R11;
    uint64_t R12;
    uint64_t R13;
    uint64_t R14;
    uint64_t R15;
    uint64_t RIP;
    uint64_t RFLAGS;
    uint64_t CR3;
    uint16_t CS;
    uint16_t SS;
    uint32_t _align; // make the struct 8 byte aligned
};

struct x86_64_ExtraContext {
    void* SIMDSaveRegion;
    uint64_t fsBase;
    uint64_t gsBase;
};

struct [[gnu::packed, gnu::aligned(16)]] FXSaveRegion {
    uint16_t FCW;
    uint16_t FSW;
    uint8_t FTW;
    uint8_t reserved0;
    uint16_t FOP;
    uint64_t FIP;
    uint64_t FDP;
    uint32_t MXCSR;
    uint32_t MXCSR_MASK;
    struct {
        uint64_t data;
        uint64_t reserved;
    } mm[8];
    struct {
        uint64_t data[2];
    } xmm[16];
    uint64_t reserved1[6];
    uint64_t available[6];
};

#ifdef __cplusplus
extern "C" {
#endif

void x86_64_DisableInterrupts();
void x86_64_EnableInterrupts();

uint8_t x86_64_GetLAPICID();

#ifdef __cplusplus
}
#endif

#endif /* _x86_64_ARCH_DEFINTIONS_H */