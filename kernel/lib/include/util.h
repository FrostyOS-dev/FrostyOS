/*
Copyright (©) 2022-2026  Frosty515

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

#ifndef _KERNEL_UTIL_H
#define _KERNEL_UTIL_H

#include <stdint.h>
#include <stddef.h>

#define KiB(x) ((uint64_t)x * (uint64_t)1024)
#define MiB(x) (KiB(x) * (uint64_t)1024)
#define GiB(x) (MiB(x) * (uint64_t)1024)

#ifndef KERNEL_STACK_SIZE
// Size of the kernel stack after init
#define KERNEL_STACK_SIZE 16384
#endif

#define volatile_read8(x) (*(volatile uint8_t*)&(x))
#define volatile_read16(x) (*(volatile uint16_t*)&(x))
#define volatile_read32(x) (*(volatile uint32_t*)&(x))
#define volatile_read64(x) (*(volatile uint64_t*)&(x))

#define volatile_write8(x, y) (*(volatile uint8_t*)&(x) = (y))
#define volatile_write16(x, y) (*(volatile uint16_t*)&(x) = (y))
#define volatile_write32(x, y) (*(volatile uint32_t*)&(x) = (y))
#define volatile_write64(x, y) (*(volatile uint64_t*)&(x) = (y))

#define volatile_addr_read8(x) (*(volatile uint8_t*)(x))
#define volatile_addr_read16(x) (*(volatile uint16_t*)(x))
#define volatile_addr_read32(x) (*(volatile uint32_t*)(x))
#define volatile_addr_read64(x) (*(volatile uint64_t*)(x))

#define volatile_addr_write8(x, y) (*(volatile uint8_t*)(x) = (y))
#define volatile_addr_write16(x, y) (*(volatile uint16_t*)(x) = (y))
#define volatile_addr_write32(x, y) (*(volatile uint32_t*)(x) = (y))
#define volatile_addr_write64(x, y) (*(volatile uint64_t*)(x) = (y))

#ifdef __cplusplus
extern "C" {
#endif

#define BCD_TO_BINARY(x) (((x & 0xF0) >> 1) + ((x & 0xF0) >> 3) + (x & 0xF))

#define DIV_ROUNDUP(VALUE, DIV) ((VALUE) + ((DIV) - 1)) / (DIV)

#define SHR_ROUNDUP(VALUE, DIV) ((VALUE) + ((DIV) - 1)) >> (DIV)

#define DIV_ROUNDUP_ADDRESS(ADDR, DIV) (void*)(((unsigned long)(ADDR) + ((DIV) - 1)) / (DIV))

#define ALIGN_UP(VALUE, ALIGN) DIV_ROUNDUP((VALUE), (ALIGN)) * (ALIGN)

#define ALIGN_DOWN(VALUE, ALIGN) ((VALUE) / (ALIGN)) * (ALIGN)

#define ALIGN_ADDRESS_DOWN(ADDR, ALIGN) (void*)(((unsigned long)(ADDR) / (ALIGN)) * (ALIGN))

#define ALIGN_ADDRESS_UP(ADDR, ALIGN) (void*)((((unsigned long)(ADDR) + ((ALIGN) - 1)) / (ALIGN)) * (ALIGN))

#define PAGE_SIZE 4096
#define PAGE_SIZE_SHIFT 12

#define LARGE_PAGE_SIZE 2097152
#define LARGE_PAGE_SIZE_SHIFT 21

#define HUGE_PAGE_SIZE 1073741824
#define HUGE_PAGE_SIZE_SHIFT 30

#define IN_BOUNDS(VALUE, LOW, HIGH) (LOW <= VALUE && VALUE <= HIGH)

#define TODO() __assert_fail("TODO", __FILE__, __LINE__, __ASSERT_FUNCTION)

void* memset(void* dst, const int value, const size_t n);
void* memcpy(void* dst, const void* src, const size_t n);
void* memmove(void* dst, const void* src, const size_t n);

int memcmp(const void* s1, const void* s2, const size_t n);
bool memcmp_b(const void* s, uint8_t c, const size_t n);

#define FLAG_SET(x, flag) x |= (flag)
#define FLAG_UNSET(x, flag) x &= ~(flag)

#ifdef __cplusplus
}
#endif

#endif /*_KERNEL_UTIL_H */
