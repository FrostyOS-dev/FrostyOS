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

#include "IPI.hpp"

#include "LAPIC.hpp"

void x86_64_Request_IPI(x86_64_LAPIC* LAPIC, uint8_t vector, x86_64_IPI_Delivery_Mode delivery_mode, x86_64_IPI_Destination_Mode destination_mode, x86_64_IPI_Destination_Shorthand destination_shorthand, x86_64_IPI_Level level, x86_64_IPI_Trigger_Mode trigger_mode, uint8_t destination) {
    uint32_t icr_low = LAPIC->ReadRegister(x86_64_LAPIC_Register::ICR);
    uint32_t icr_high = LAPIC->ReadRegister((uint64_t)x86_64_LAPIC_Register::ICR + 0x10);

    icr_low |= (uint32_t)vector & 0xFF;
    icr_low |= ((uint32_t)delivery_mode & 7) << 8;
    icr_low |= ((uint32_t)destination_mode & 1) << 11;
    icr_low |= ((uint32_t)level & 1) << 14;
    icr_low |= ((uint32_t)trigger_mode & 1) << 15;
    icr_low |= ((uint32_t)destination_shorthand & 3) << 18;
    icr_high |= ((uint32_t)destination & 0xFF) << 24;


    LAPIC->WriteRegister((uint64_t)x86_64_LAPIC_Register::ICR + 0x10, icr_high);
    LAPIC->WriteRegister(x86_64_LAPIC_Register::ICR, icr_low);
}
