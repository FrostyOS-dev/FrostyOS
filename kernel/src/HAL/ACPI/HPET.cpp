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

#include "HPET.hpp"

#include "../drivers/HPET.hpp"

#include <assert.h>
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/acpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

#pragma GCC diagnostic pop

bool InitHPET() {
    uacpi_table table;
    uacpi_status rc = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);
    if (uacpi_unlikely_error(rc))
        return false;

    acpi_hpet* HPET_table = static_cast<acpi_hpet*>(table.ptr);
    assert(HPET_table != nullptr);

    if (HPET_table->address.address_space_id != 0)
        return false;

    HPET* hpet = new HPET(HPET_table->address.address);
    if (!hpet->Init())
        return false;

    g_HPET = hpet;

    return true;
}
