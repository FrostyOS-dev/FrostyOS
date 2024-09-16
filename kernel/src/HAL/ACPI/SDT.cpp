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

#include "SDT.hpp"

#include <stdint.h>
#include <stddef.h>

namespace ACPI {

    bool ValidateSDT(SDTHeader* sdt) {
        uint8_t Checksum = 0;
        for (size_t i = 0; i < sdt->Length; i++)
            Checksum += ((uint8_t*)sdt)[i];
        
        return (Checksum & 0xFF) == 0;
    }

}