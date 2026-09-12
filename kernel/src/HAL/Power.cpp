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

#include "Power.hpp"
#include "Processor.hpp"

#include "ACPI/Sleep.hpp"

int HAL_PrepareForPowerOff() {
    // TODO: FS should be safely unmounted, along with other cleanup

    Processor::DisableInterrupts();
    Processor* proc = GetCurrentProcessor();

    proc->HaltAllExclSelf(); // halt all other processors. Ideally we are running on the BSP

    return 0;
}

int HAL_ShutdownSystem() {
    int rc = HAL_PrepareForPowerOff();
    if (rc < 0)
        return rc;

    return ACPI::Shutdown();
}

int HAL_RebootSystem() {
    int rc = HAL_PrepareForPowerOff();
    if (rc < 0)
        return rc;

    return ACPI::Reboot();
}
