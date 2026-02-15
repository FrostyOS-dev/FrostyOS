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
#include "MADT.hpp"
#include "MCFG.hpp"

#include "../HAL.hpp"

#include <stdio.h>
#include <util.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/context.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>

#pragma GCC diagnostic pop

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

namespace ACPI {
    void* g_RSDP = nullptr;

    void EarlyInit(void* RSDP) {
        g_RSDP = from_HHDM(RSDP);

#ifdef UACPI_BAREBONES_MODE
        
        uacpi_context_set_log_level(UACPI_LOG_INFO);

        uacpi_status rc = uacpi_setup_early_table_access(to_HHDM(g_PMM->AllocatePage()), PAGE_SIZE);
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to setup early table access: %d\n", rc);
            PANIC("ACPI: Failed to setup early table access");
        }

#else

        uacpi_status rc = uacpi_initialize(0);
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to initialize uACPI: %d\n", rc);
            PANIC("ACPI: Failed to initialize uACPI");
        }

        if (!InitMADT()) {
            dbgprintf("MADT init failed.\n");
            PANIC("Failed to initialise MADT table");
        }

        if (!InitMCFG()) {
            dbgprintf("MCFG init failed.\n");
            PANIC("Failed to initialise MCFG table");
        }

        dbgprintf("ACPI: Early init complete\n");
#endif
    }

    void Stage2Init() {
#ifndef UACPI_BAREBONES_MODE
        uacpi_status rc = uacpi_namespace_load();
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to load ACPI namespace: %d\n", rc);
            PANIC("ACPI: Failed to load ACPI namespace");
        }

        rc = uacpi_namespace_initialize();
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to initialize ACPI namespace: %d\n", rc);
            PANIC("ACPI: Failed to initialize ACPI namespace");
        }

        dbgprintf("ACPI: Stage 2 init complete\n");
#endif
    }

    void* GetRSDP() {
        return g_RSDP;
    }
}