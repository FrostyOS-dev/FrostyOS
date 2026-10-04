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

#ifndef _FROSTYOS_ASM_VIDEO_H
#define _FROSTYOS_ASM_VIDEO_H

#define FOSV_GET_DISPLAY_INFO 0x46564400
#define FOSV_GET_DISPLAY_MODE 0x46564401
#define FOSV_SET_DISPLAY_MODE 0x46564402

typedef unsigned int _u32;
typedef unsigned long int _u64;

enum FOSV_PixelFormat {
    FOSV_PIXFORM_RGB565   = 0x46565000,
    FOSV_PIXFORM_XRGB8888 = 0x46565001,
    FOSV_PIXFORM_ARGB8888 = 0x46565002,
    FOSV_PIXFORM_BGRX8888 = 0x46565003,
    FOSV_PIXFORM_BGRA8888 = 0x46565004,
    FOSV_PIXFORM_RGBX8888 = 0x46565005,
    FOSV_PIXFORM_RGBA8888 = 0x46565006,
    FOSV_PIXFORM_UNKNOWN  = 0x465650FF
};

enum FOSV_DisplayCap {
    FOSV_DCAP_LINEAR_FB     = 0x46564480,
    FOSV_DCAP_MODESET       = 0x46564481,
    FOSV_DCAP_ACCURATE_TIME = 0x46564482 // Accurate pixel timing
};

typedef struct FOSV_VideoMode {
    _u32 width;
    _u32 height;
    _u32 pitch;
    _u32 refreshRate;
    _u64 pixelClock;
    _u32 pixelFormat;
} FOSV_VideoMode;

typedef struct FOSV_DisplayInfo {
    _u64 cap; // Capabilities bitmap
    _u32 modeCount;
    FOSV_VideoMode currentMode;
    FOSV_VideoMode nativeMode;
} FOSV_DisplayInfo;

typedef struct FOSV_GetDisplayModeArg {
    _u32 mode;
    FOSV_VideoMode info;
} FOSV_GetDisplayModeArg;

#endif /* _FROSTYOS_ASM_VIDEO_H */