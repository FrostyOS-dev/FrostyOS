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

#ifndef _FROSTYOS_ASM_INPUT_H
#define _FROSTYOS_ASM_INPUT_H

#include "types.h"

#define FOSI_GET_DEVICE_INFO 0x46494400

enum FOSI_DeviceType {
    FOSI_DT_KEYBOARD = 0x46495400
};

enum FOSI_EventType {
    FOSI_ET_KEYBOARDKEY = 0x46494500
};

typedef struct FOSI_DeviceInfo {
    _u32 type;
    _u32 cap; // Capabilities bitmap
} FOSI_DeviceInfo;

typedef struct FOSI_Event {
    _u32 type;
    _u32 code;
    _u64 data;
} FOSI_Event;

#endif /* _FROSTYOS_ASM_INPUT_H */