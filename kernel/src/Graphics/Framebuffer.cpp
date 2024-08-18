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

#include "Framebuffer.hpp"

void WriteToFrameBuffer(FrameBuffer* fb, uint64_t x, uint64_t y, Colour colour) {
    void* pixel = (uint64_t*)((uint64_t)fb->BaseAddress + (y * fb->pitch) + (x * (fb->bpp / 8)));
    uint64_t raw_colour = colour.Render(fb->bpp, fb->red_mask, fb->red_shift, fb->green_mask, fb->green_shift, fb->blue_mask, fb->blue_shift);
    switch(fb->bpp) {
        case 64: {
            uint64_t* pixel64 = (uint64_t*)pixel;
            *pixel64 = raw_colour;
            break;
        }
        case 56: {
            // merge 32 + 16 + 8 bits
            uint32_t* pixel32 = (uint32_t*)pixel;
            uint16_t* pixel16 = (uint16_t*)((uint64_t)pixel + 4);
            uint8_t* pixel8 = (uint8_t*)((uint64_t)pixel + 6);
            *pixel32 = raw_colour & 0xFFFFFFFF;
            *pixel16 = (raw_colour >> 32) & 0xFFFF;
            *pixel8 = (raw_colour >> 48) & 0xFF;
            break;
        }
        case 48: {
            // merge 32 + 16 bits
            uint32_t* pixel32 = (uint32_t*)pixel;
            uint16_t* pixel16 = (uint16_t*)((uint64_t)pixel + 4);
            *pixel32 = raw_colour & 0xFFFFFFFF;
            *pixel16 = (raw_colour >> 32) & 0xFFFF;
            break;
        }
        case 40: {
            // merge 32 + 8 bits
            uint32_t* pixel32 = (uint32_t*)pixel;
            uint8_t* pixel8 = (uint8_t*)((uint64_t)pixel + 4);
            *pixel32 = raw_colour & 0xFFFFFFFF;
            *pixel8 = (raw_colour >> 32) & 0xFF;
            break;
        }
        case 32: {
            uint32_t* pixel32 = (uint32_t*)pixel;
            *pixel32 = raw_colour;
            break;
        }
        case 24: {
            // merge 16 + 8 bits
            uint16_t* pixel16 = (uint16_t*)pixel;
            uint8_t* pixel8 = (uint8_t*)((uint64_t)pixel + 2);
            *pixel16 = raw_colour & 0xFFFF;
            *pixel8 = (raw_colour >> 16) & 0xFF;
            break;
        }
        case 16: {
            uint16_t* pixel16 = (uint16_t*)pixel;
            *pixel16 = raw_colour;
            break;
        }
        case 8: {
            uint8_t* pixel8 = (uint8_t*)pixel;
            *pixel8 = raw_colour;
            break;
        }
    }
}

void WriteToFrameBuffer(FrameBuffer* fb, uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b) {
    WriteToFrameBuffer(fb, x, y, Colour(r, g, b));
}

void ClearFrameBuffer(FrameBuffer* fb, Colour colour) {
    for (uint64_t y = 0; y < fb->height; y++) {
        for (uint64_t x = 0; x < fb->width; x++)
            WriteToFrameBuffer(fb, x, y, colour);
    }
}