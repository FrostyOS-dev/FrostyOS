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

#include "Init.hpp"
#include "spinlock.h"
#include "uacpi/types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <Memory/PagingUtil.hpp>

#include <uacpi/kernel_api.h>
#include <uacpi/status.h>

#include <uacpi/platform/types.h>

#ifdef __x86_64__
#include <arch/x86_64/ArchDefs.h>
#include <arch/x86_64/IO.h>
#else
#error Invalid architecture
#endif

#ifdef __cplusplus
extern "C" {
#endif

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    *out_rsdp_address = (uacpi_phys_addr)HHDM_to_phys(ACPI::GetRSDP());
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_read(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64* out_value) {
    void* virt_addr = to_HHDM((void*)address);
    switch (byte_width) {
        case 1:
            *out_value = *(uacpi_u8*)virt_addr;
            break;
        case 2:
            *out_value = *(uacpi_u16*)virt_addr;
            break;
        case 4:
            *out_value = *(uacpi_u32*)virt_addr;
            break;
        case 8:
            *out_value = *(uacpi_u64*)virt_addr;
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 in_value) {
    void* virt_addr = to_HHDM((void*)address);
    switch (byte_width) {
        case 1:
            *(uacpi_u8*)virt_addr = (uacpi_u8)in_value;
            break;
        case 2:
            *(uacpi_u16*)virt_addr = (uacpi_u16)in_value;
            break;
        case 4:
            *(uacpi_u32*)virt_addr = (uacpi_u32)in_value;
            break;
        case 8:
            *(uacpi_u64*)virt_addr = (uacpi_u64)in_value;
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}


#ifdef __x86_64__

uacpi_status uacpi_kernel_raw_io_read(uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64* out_value) {
    switch (byte_width) {
        case 1:
            *out_value = x86_64_inb(address);
            break;
        case 2:
            *out_value = x86_64_inw(address);
            break;
        case 4:
            *out_value = x86_64_ind(address);
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 in_value) {
    switch (byte_width) {
        case 1:
            x86_64_outb(address, (uacpi_u8)in_value);
            break;
        case 2:
            x86_64_outw(address, (uacpi_u16)in_value);
            break;
        case 4:
            x86_64_outd(address, (uacpi_u32)in_value);
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

#endif

uacpi_status uacpi_kernel_pci_read(uacpi_pci_address* address, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64* value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write(uacpi_pci_address* address, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64 value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

namespace ACPI {
    struct SystemIO {
        uacpi_io_addr address;
        uacpi_size size;
    };
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr address, uacpi_size size, uacpi_handle* out_handle) {
    ACPI::SystemIO* io = new ACPI::SystemIO();
    io->address = address;
    io->size = size;
    *out_handle = (uacpi_handle)io;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    delete (ACPI::SystemIO*)handle;
}

uacpi_status uacpi_kernel_io_read(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64* value) {
    ACPI::SystemIO* io = (ACPI::SystemIO*)handle;
    if (offset >= io->size)
        return UACPI_STATUS_INVALID_ARGUMENT;
    return uacpi_kernel_raw_io_read(io->address + offset, byte_width, value);
}

uacpi_status uacpi_kernel_io_write(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64 value) {
    ACPI::SystemIO* io = (ACPI::SystemIO*)handle;
    if (offset >= io->size)
        return UACPI_STATUS_INVALID_ARGUMENT;
    return uacpi_kernel_raw_io_write(io->address + offset, byte_width, value);
}

// TODO: Maybe properly map into kernel space instead of HHDM

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    return to_HHDM((void*)addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {
    // Do nothing
}

void* uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc(size);
}

void* uacpi_kernel_calloc(uacpi_size count, uacpi_size size) {
    return kcalloc(count, size);
}

void uacpi_kernel_free(void* mem) {
    kfree(mem);
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* msg, ...) {
    va_list args;
    va_start(args, msg);
    uacpi_kernel_vlog(level, msg, args);
    va_end(args);
}

void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char* msg, uacpi_va_list args) {
    switch (level) {
        case UACPI_LOG_DEBUG:
            dbgprintf("[uACPI/DEBUG]: ");
            break;
        case UACPI_LOG_TRACE:
            dbgprintf("[uACPI/TRACE]: ");
            break;
        case UACPI_LOG_INFO:
            dbgprintf("[uACPI/INFO]: ");
            break;
        case UACPI_LOG_WARN:
            dbgprintf("[uACPI/WARN]: ");
            break;
        case UACPI_LOG_ERROR:
            dbgprintf("[uACPI/ERROR]: ");
            break;
    }
    dbgvprintf(msg, args);
}

uacpi_u64 uacpi_kernel_get_ticks() {
    return 0;
}

void uacpi_kernel_stall(uacpi_u8) {

}

void uacpi_kernel_sleep(uacpi_u64) {

}

uacpi_handle uacpi_kernel_create_mutex() {
    return new uint64_t;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex) {
    delete (uint64_t*)mutex;
}

uacpi_handle uacpi_kernel_create_event() {
    return nullptr;
}

void uacpi_kernel_free_event(uacpi_handle) {

}

uacpi_thread_id uacpi_kernel_get_thread_id() {
    return 0;
}

uacpi_bool uacpi_kernel_acquire_mutex(uacpi_handle, uacpi_u16) {
    return UACPI_TRUE;
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

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx, uacpi_handle* out_irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler, uacpi_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_handle uacpi_kernel_create_spinlock() {
    return new spinlock_t(SPINLOCK_DEFAULT_VALUE);
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    delete (spinlock_t*)handle;
}

#ifdef __x86_64__
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    uacpi_cpu_flags flags = x86_64_DisableInterruptsWithSave();
    spinlock_t* lock = (spinlock_t*)handle;
    spinlock_acquire(lock);
    return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    spinlock_t* lock = (spinlock_t*)handle;
    spinlock_release(lock);
    x86_64_EnableInterruptsWithSave(flags);
}
#endif

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler, uacpi_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_wait_for_work_completion() {
    return UACPI_STATUS_UNIMPLEMENTED;
}

#ifdef __cplusplus
}
#endif