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

#include "Init.hpp"

#include <stdarg.h>
#include <stdio.h>

#ifdef __x86_64__
#include <arch/x86_64/Memory/PageTables.hpp>
#include <arch/x86_64/Memory/PagingInit.hpp>
#endif

#include <uacpi/kernel_api.h>

#include <Memory/PagingUtil.hpp>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    if (out_rsdp_address == nullptr)
        return UACPI_STATUS_INVALID_ARGUMENT;

    *out_rsdp_address = (uint64_t)ACPI::GetRSDP();
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr phys_addr, size_t size) {
#ifdef __x86_64__
    x86_64_MapPage(g_KernelRootPageTable, to_HHDM(phys_addr), phys_addr, 0x08000003);
#endif
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