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
#include "MADT.hpp"

#include <stdio.h>

#include <Memory/PagingUtil.hpp>

#include <uacpi/context.h>
#include <uacpi/event.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

namespace ACPI {

    void* g_RSDP = nullptr;

    void EarlyInit(void* rsdp) {
        g_RSDP = rsdp;

        
    }

    void BaseInit() {
        uacpi_context_set_log_level(UACPI_LOG_TRACE);

        uacpi_status rc = uacpi_initialize(0);
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to initialize uACPI: %s\n", uacpi_status_to_string(rc));
            return;
        }

        uacpi_table madt;
        rc = uacpi_table_find_by_signature("APIC", &madt);
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to find MADT: %s\n", uacpi_status_to_string(rc));
            return;
        }

        InitMADT((MADT*)madt.hdr);
    }

    void FullInit() {
        uacpi_status rc = uacpi_namespace_load();
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to load ACPI namespace.\n");
            return;
        }

        rc = uacpi_namespace_initialize();
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to initialize ACPI namespace.\n");
            return;
        }

        rc = uacpi_finalize_gpe_initialization();
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to finalize GPE initialization.\n");
            return;
        }
    }

    void* GetRSDP() {
        return g_RSDP;
    }

}