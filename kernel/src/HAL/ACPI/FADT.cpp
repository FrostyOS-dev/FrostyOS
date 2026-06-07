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

#include "FADT.hpp"

#include <assert.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/acpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

#pragma GCC diagnostic pop

#ifdef __x86_64__
#include <arch/x86_64/CMOS.hpp>
#endif

bool InitFADT() {
    uacpi_table table;
    uacpi_status rc = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);
    if (uacpi_unlikely_error(rc))
        return false;

    acpi_fadt* FADT = static_cast<acpi_fadt*>(table.ptr);
    assert(FADT != nullptr);

#ifdef __x86_64__
    if ((FADT->iapc_boot_arch & ACPI_IA_PC_NO_CMOS_RTC) == 0)
        x86_64_InitCMOS(FADT->century);
#endif

    return true;
}
