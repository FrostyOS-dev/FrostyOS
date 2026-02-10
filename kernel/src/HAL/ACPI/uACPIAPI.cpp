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

#include "Init.hpp"

#include <spinlock.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <util.h>

#ifdef __x86_64__
#include <arch/x86_64/ArchDefs.h>
#include <arch/x86_64/IO.h>

#include <arch/x86_64/Memory/PageTables.hpp>
#include <arch/x86_64/Memory/PagingInit.hpp>
#endif

#include <uacpi/kernel_api.h>

#include <HAL/Time.hpp>

#include <Memory/PagingUtil.hpp>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    if (out_rsdp_address == nullptr)
        return UACPI_STATUS_INVALID_ARGUMENT;

    *out_rsdp_address = (uint64_t)ACPI::GetRSDP();
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr phys_addr, size_t size) {
    uint64_t start = ALIGN_DOWN(phys_addr, PAGE_SIZE);
    uint64_t end = ALIGN_UP(phys_addr + size, PAGE_SIZE);
    for (uint64_t i = start; i < end; i += PAGE_SIZE) {
#ifdef __x86_64__
        x86_64_MapPage(g_KernelRootPageTable, to_HHDM(i), i, 0x08000003);
#endif
    }
    return (void*)to_HHDM(phys_addr);
}

void uacpi_kernel_unmap(void* virt_addr, size_t size) {
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* format, ...) {
    va_list args;
    va_start(args, format);
    uacpi_kernel_vlog(level, format, args);
    va_end(args);
}

void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char* format, uacpi_va_list args) {
    switch (level) {
        case UACPI_LOG_DEBUG:
            dbgputs("[uACPI DEBUG] ");
            break;
        case UACPI_LOG_TRACE:
            dbgputs("[uACPI TRACE] ");
            break;
        case UACPI_LOG_INFO:
            dbgputs("[uACPI INFO] ");
            break;
        case UACPI_LOG_WARN:
            dbgputs("[uACPI WARN] ");
            break;
        case UACPI_LOG_ERROR:
            dbgputs("[uACPI ERROR] ");
            break;
        default:
            break;
    }

    dbgvprintf(format, args);
}

#ifndef UACPI_BAREBONES_MODE

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address, uacpi_handle*) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

void uacpi_kernel_pci_device_close(uacpi_handle) {
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle, size_t, uacpi_u8*) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle, size_t, uacpi_u16*) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle, size_t, uacpi_u32*) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle, size_t, uacpi_u8) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle, size_t, uacpi_u16) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle, size_t, uacpi_u32) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

struct uACPIAPI_IOHandle {
    uacpi_io_addr start;
    size_t length;
};

uacpi_status uacpi_kernel_io_map(uacpi_io_addr start, size_t length, uacpi_handle* out_handle) {
    uACPIAPI_IOHandle* handle = (uACPIAPI_IOHandle*)kcalloc(1, sizeof(uACPIAPI_IOHandle));
    handle->start = start;
    handle->length = length;
    *out_handle = handle;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    kfree(handle);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, size_t offset, uacpi_u8* data) {
#ifdef __x86_64__
    *data = x86_64_inb(((uACPIAPI_IOHandle*)handle)->start + offset);
#endif
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, size_t offset, uacpi_u16* data) {
#ifdef __x86_64__
    *data = x86_64_inw(((uACPIAPI_IOHandle*)handle)->start + offset);
#endif
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, size_t offset, uacpi_u32* data) {
#ifdef __x86_64__
    *data = x86_64_ind(((uACPIAPI_IOHandle*)handle)->start + offset);
#endif
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, size_t offset, uacpi_u8 data) {
#ifdef __x86_64__
    x86_64_outb(((uACPIAPI_IOHandle*)handle)->start + offset, data);
#endif
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, size_t offset, uacpi_u16 data) {
#ifdef __x86_64__
    x86_64_outw(((uACPIAPI_IOHandle*)handle)->start + offset, data);
#endif
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, size_t offset, uacpi_u32 data) {
#ifdef __x86_64__
    x86_64_outd(((uACPIAPI_IOHandle*)handle)->start + offset, data);
#endif
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc(size);
}

#ifdef UACPI_NATIVE_ALLOC_ZEROED

void* uacpi_kernel_alloc_zeroed(uacpi_size size) {
    return kcalloc(1, size);
}

#endif /* UACPI_NATIVE_ALLOC_ZEROED */

void uacpi_kernel_free(void* mem) {
    kfree(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return HAL_GetTicks() * 1'000'000;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    uint64_t msec = DIV_ROUNDUP(usec, 1000);
    uint64_t start = HAL_GetTicks();
    while (HAL_GetTicks() - start < msec);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    uint64_t start = HAL_GetTicks();
    while (HAL_GetTicks() - start < msec);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
    return kcalloc(1, 8);
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    kfree(handle);
}

uacpi_handle uacpi_kernel_create_event() {
    return kcalloc(1, 8);
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    kfree(handle);
}

uacpi_thread_id uacpi_kernel_get_thread_id() {
    return 0;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle, uacpi_u16) {
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle) {
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle) {
}

void uacpi_kernel_reset_event(uacpi_handle) {
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle* out_irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_t* lock = (spinlock_t*)kmalloc(sizeof(spinlock_t));
    *lock = SPINLOCK_DEFAULT_VALUE;
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    kfree(lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    spinlock_acquire((spinlock_t*)lock);
#ifdef __x86_64__
    uint64_t flags = 0;
    __asm__ volatile("pushfq\npop %0\ncli" : "=r"(flags));
    return flags & (1 << 9);
#else
    return 0;
#endif
}

void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags flags) {
#ifdef __x86_64__
    if (flags & (1 << 9))
        x86_64_EnableInterrupts();
#endif
    spinlock_release((spinlock_t*)lock);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler, uacpi_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_wait_for_work_completion() {
    return UACPI_STATUS_UNIMPLEMENTED;
}

#endif /* UACPI_BAREBONES_MODE */