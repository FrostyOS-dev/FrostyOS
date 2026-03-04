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

#include "IPI.hpp"
#include "LocalAPIC.hpp"

namespace x86_64_IPI {
    void RaiseIPI(x86_64_LAPIC* lapic, uint8_t dest, uint8_t vector, DeliveryMode deliveryMode, TriggerMode mode, DestMode destMode, DestShort shorthand, Level level) {
        if (lapic == nullptr)
            return;

        uint64_t value = 0;
        uint32_t* temp = reinterpret_cast<uint32_t*>(&value);
        temp[0] = lapic->ReadRegister(x86_64_LAPIC_Register::ICR0);
        temp[1] = lapic->ReadRegister(x86_64_LAPIC_Register::ICR1);

        value &= ~(0xFFUL << 56 | 0xCCFFF);
        value |= (uint64_t)dest << 56;
        value |= vector;
        value |= (uint64_t)deliveryMode << 8;
        value |= (uint64_t)mode << 15;
        value |= (uint64_t)destMode << 11;
        value |= (uint64_t)shorthand << 18;
        value |= (uint64_t)level << 14;

        lapic->WriteRegister(x86_64_LAPIC_Register::ICR1, temp[1]);
        lapic->WriteRegister(x86_64_LAPIC_Register::ICR0, temp[0]); // writing the lower dword triggers the IPI
    
        while ((lapic->ReadRegister(x86_64_LAPIC_Register::ICR0) & (1 << 12)) > 0) { // wait for it to be delivered
        }
    }
}
