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

#include "../HAL.hpp"

#include <stdio.h>
#include <util.h>

#include <uacpi/status.h>
#include <uacpi/uacpi.h>

#include <Memory/PagingUtil.hpp>
#include <Memory/PMM.hpp>

namespace ACPI {
    void* g_RSDP = nullptr;

    void EarlyInit(void* RSDP) {
        g_RSDP = from_HHDM(RSDP);

        uacpi_status rc = uacpi_setup_early_table_access(to_HHDM(g_PMM->AllocatePage()), PAGE_SIZE);
        if (uacpi_unlikely_error(rc)) {
            dbgprintf("Failed to setup early table access: %d\n", rc);
            PANIC("ACPI: Failed to setup early table access");
        }

        dbgprintf("ACPI: Early init complete\n");
    }

    void* GetRSDP() {
        return g_RSDP;
    }
}