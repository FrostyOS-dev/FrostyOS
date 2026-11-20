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

#include "key.h"
#include "types.h"

#define FOSI_GET_DEVICE_INFO 0x46494400
#define FOSI_SET_CLAIM       0x46494401
#define FOSI_SET_CLOCK       0x46494402
#define FOSI_GET_KEY_STATE   0x46494403

enum FOSI_DeviceType {
    FOSI_DT_KEYBOARD = 0x46495400,
    FOSI_DT_MOUSE    = 0x46495401
};

#define FOSI_CAP_KEY (1u << 0)
#define FOSI_CAP_REL (1u << 1)

enum FOSI_EventType {
    FOSI_ET_KEY = 0x46494500,
    FOSI_ET_REL = 0x46494501,
    FOSI_ET_SYN = 0x46494502
};

enum FOSI_KeyValue {
    FOSI_KV_RELEASE = 0,
    FOSI_KV_PRESS   = 1,
    FOSI_KV_REPEAT  = 2
};

enum FOSI_REL_EventCode {
    FOSI_REL_X      = 0, // positive = right
    FOSI_REL_Y      = 1, // positive = down
    FOSI_REL_WHEEL  = 2, // positive = away from user (scroll up)
    FOSI_REL_HWHEEL = 3  // positive = right
};

enum FOSI_SYN_Code {
    FOSI_SYN_REPORT = 0,
    FOSI_SYN_DROPPED = 1
};

enum FOSI_Clock {
    FOSI_CLOCK_REALTIME = 0, // default. Wall clock, tv_sec/tv_nsec since unix epoch.
    FOSI_CLOCK_MONOTONIC = 1 // time since boot
};

typedef struct FOSI_DeviceInfo {
    _u32 type;
    _u32 cap; // FOSI_CAP_* bitmap
} FOSI_DeviceInfo;

typedef struct FOSI_Event {
    _i64 tv_sec;
    _i64 tv_nsec;
    _u32 type;
    _u32 code;
    _i64 value;
} FOSI_Event;

typedef struct FOSI_KeyState {
    _u8 bits[FOSI_KEY_BITMAP_BYTES];
} FOSI_KeyState;

#endif /* _FROSTYOS_ASM_INPUT_H */