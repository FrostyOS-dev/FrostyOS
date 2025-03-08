/*
Copyright (©) 2025  Frosty515

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

#ifndef _x86_64_MSR_H
#define _x86_64_MSR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum MSRs {
    // Time Stamp Counter
    MSR_TSC = 0x10,
    MSR_TSC_ADJUST = 0x3B,
    MSR_TSC_AUX = 0xC0000103,
    MSR_TSC_DEADLINE = 0x6E0,
    MSR_MPERF = 0xE7,
    MSR_APERF = 0xE8,

    // Feature control
    MSR_INTEL_P5_TR12 = 0x3,
    MSR_MISC_ENABLE = 0x1A0,
    MSR_PREFETCH_CTRL = 0x1A4,
    MSR_CODE_PREF_CTRL = 0x1A9,
    MSR_CODE_CAPABILITIES = 0xCF,
    MSR_MEMORY_CTRL = 0x33,
    MSR_EFER = 0xC0000080,
    MSR_AMD_HWCR = 0xC0010015,
    MSR_PLATFORM_INFO = 0xCE,
    MSR_MISC_FEATURES = 0x140,

    // Speculation control
    MSR_ARCH_CAPABILITIES = 0x10A,
    MSR_SPEC_CTRL = 0x48,
    MSR_PRED_CMD = 0x49,
    MSR_FLUSH_CMD = 0x10B,
    MSR_MCU_OPT_CTRL = 0x123,
    MSR_TSX_CTRL = 0x122,
    MSR_TSX_FORCE_ABORT = 0x10F,
    MSR_UARCH_MISC_CTRL = 0x1B01,

    // PSN or PPIN
    MSR_PSN_CTL = 0x119,
    MSR_PSN = 0x3,
    MSR_INTEL_PPIN_CTL = 0x4E,
    MSR_INTEL_PPIN = 0x4F,
    MSR_AMD_PPIN_CTL = 0X102F0,
    MSR_AMD_PPIN = 0x102F1,

    // SYSENTER and SYSEXIT
    MSR_SEP_SEL = 0x174,
    MSR_SEP_ESP = 0x175,
    MSR_SEP_RSP = 0x176,
    MSR_SEP_EIP = 0x177,
    MSR_SEP_RIP = 0x178,

    // SYSCALL and SYSRET
    MSR_STAR = 0xC0000081,
    MSR_LSTAR = 0xC0000082,
    MSR_CSTAR = 0xC0000083,
    MSR_FMASK = 0xC0000084,

    // FS base and GS base
    MSR_FS_BASE = 0xC0000100,
    MSR_GS_BASE = 0xC0000101,
    MSR_KERNEL_GS_BASE = 0xC0000102,

    // Page Attribute Table
    MSR_PAT = 0x277,

    // Memory Type Range Registers
    MSR_MTRR_CAP = 0xFE,
    MSR_MTRR_DEF_TYPE = 0x2FF,

    // Fixed range MTRRs
    MSR_MTRR_FIX64K_00000 = 0x250,
    MSR_MTRR_FIX16K_80000 = 0x258,
    MSR_MTRR_FIX16K_A0000 = 0x259,
    MSR_MTRR_FIX4K_C0000 = 0x268,
    MSR_MTRR_FIX4K_C8000 = 0x269,
    MSR_MTRR_FIX4K_D0000 = 0x26A,
    MSR_MTRR_FIX4K_D8000 = 0x26B,
    MSR_MTRR_FIX4K_E0000 = 0x26C,
    MSR_MTRR_FIX4K_E8000 = 0x26D,
    MSR_MTRR_FIX4K_F0000 = 0x26E,
    MSR_MTRR_FIX4K_F8000 = 0x26F,

    // Variable range MTRRs
    MSR_MTRR_PHYS_BASE_0 = 0x200,
    MSR_MTRR_PHYS_MASK_0 = 0x201,

    // SMRRS
    MSR_SMRR_PHYS_BASE = 0x1F2,
    MSR_SMRR_PHYS_MASK = 0x1F3,

    // Machine Check Exception
    MSR_MCAR = 0x0,
    MSR_MCTR = 0x1,

    // Machine Check Architecture
    MSR_MCG_CONTAIN = 0x178,
    MSR_MCG_CAP = 0x179,
    MSR_MCG_STATUS = 0x17A,
    MSR_MCG_CTL = 0x17B,
    MSR_MCG_EXT_CTL = 0x4D0,

    // MCA Error-Reporting Register Banks
    MSR_MC0_CTL2 = 0x280,
    MSR_MC0_CTL = 0x400,
    MSR_MC0_STATUS = 0x401,
    MSR_MC0_ADDR = 0x402,
    MSR_MC0_MISC = 0x403,

    // MCA Extended State Registers
    MSR_MCG_rAX = 0x180,
    MSR_MCG_rBX = 0x181,
    MSR_MCG_rCX = 0x182,
    MSR_MCG_rDX = 0x183,
    MSR_MCG_rSI = 0x184,
    MSR_MCG_rDI = 0x185,
    MSR_MCG_rBP = 0x186,
    MSR_MCG_rSP = 0x187,
    MSR_MCG_rFLAGS = 0x188,
    MSR_MCG_rIP = 0x189,
    MSR_MCG_MISC = 0x18A,
    MSR_MCG_RES0 = 0x18B,
    MSR_MCG_R8 = 0x190,
    MSR_MCG_R9 = 0x191,
    MSR_MCG_R10 = 0x192,
    MSR_MCG_R11 = 0x193,
    MSR_MCG_R12 = 0x194,
    MSR_MCG_R13 = 0x195,
    MSR_MCG_R14 = 0x196,
    MSR_MCG_R15 = 0x197,

    // Local APIC
    MSR_APIC_BASE = 0x1B,
    MSR_XAPIC_DIS_STATUS = 0XBD,

    // BNDCFGS
    MSR_BNDCFGS = 0xD90,

    // PKRS
    MSR_PKRS = 0x6E1,

    // PASID
    MSR_PASID = 0xD93,

    // XSS
    MSR_XSS = 0xDA0,
};

uint64_t x86_64_ReadMSR(uint32_t msr);
void x86_64_WriteMSR(uint32_t msr, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* _x86_64_MSR_H */