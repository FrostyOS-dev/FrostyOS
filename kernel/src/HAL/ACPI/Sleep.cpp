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

#include "Sleep.hpp"

#include <errno.h>
#include <stdio.h>

#include <HAL/HAL.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/sleep.h>
#include <uacpi/status.h>

#pragma GCC diagnostic pop

namespace ACPI {

    int Shutdown() {
        uacpi_status rc = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
        if (uacpi_unlikely_error(rc)) {
            printf("ACPI: Failed to prepare for shutdown\n");
            return -EIO;
        }

        rc = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
        if (uacpi_unlikely_error(rc)) {
            printf("ACPI: Failed to shutdown\n");
            return -EIO;
        }

        PANIC("ACPI: Shutdown Returned!");
    }

    int Reboot() {
        // Windows does reboot via shutdown, so some hardware expects S5 prepare before reboot
        uacpi_status rc = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
        if (uacpi_unlikely_error(rc)) {
            printf("ACPI: Failed to prepare for reboot\n");
            return -EIO;
        }

        rc = uacpi_reboot();
        if (uacpi_unlikely_error(rc)) {
            printf("ACPI: Failed to reboot\n");
            return -EIO;
        }

        PANIC("ACPI: Reboot Returned!");
    }

}