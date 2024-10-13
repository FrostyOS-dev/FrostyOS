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

#include "limine.h"
#include "debug.h"
#include "kernel.hpp"

#include <Memory/MemoryMap.hpp>

extern "C" {

LIMINE_BASE_REVISION(2)

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 1,
    .response = nullptr
};

#ifdef __x86_64__
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 1,
    .response = nullptr,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL
};
#endif

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = nullptr,
    .internal_module_count = 0,
    .internal_modules = nullptr
};

void limine_request_fail(const char* request_name) {
    debug_puts("limine_request_fail: ");
    debug_puts(request_name);
    debug_puts("\n");
    while (true) {}
}

void _start() {
    if (hhdm_request.response == nullptr)
        limine_request_fail("hhdm_request");
    if (framebuffer_request.response == nullptr)
        limine_request_fail("framebuffer_request");
    if (memmap_request.response == nullptr)
        limine_request_fail("memmap_request");
    if (rsdp_request.response == nullptr)
        limine_request_fail("rsdp_request");
    if (kernel_address_request.response == nullptr)
        limine_request_fail("kernel_address_request");
    if (module_request.response == nullptr)
        limine_request_fail("module_request");

#ifdef __x86_64__
    if (paging_mode_request.response == nullptr)
        limine_request_fail("paging_mode_request");
#endif

    if (framebuffer_request.response->framebuffer_count == 0)
        limine_request_fail("framebuffer_count");

    if (module_request.response->module_count != 1)
        limine_request_fail("module_count");

    limine_framebuffer* fb = framebuffer_request.response->framebuffers[0];

    g_kernelParams.HHDMStart = hhdm_request.response->offset;
    g_kernelParams.framebuffer = {
        fb->address,
        fb->width,
        fb->height,
        fb->pitch,
        fb->bpp,
        (uint8_t)((1 << fb->red_mask_size) - 1),
        fb->red_mask_shift,
        (uint8_t)((1 << fb->green_mask_size) - 1),
        fb->green_mask_shift,
        (uint8_t)((1 << fb->blue_mask_size) - 1),
        fb->blue_mask_shift
    };
    g_kernelParams.MemoryMap = (MemoryMapEntry**)memmap_request.response->entries;
    g_kernelParams.MemoryMapEntryCount = memmap_request.response->entry_count;
    g_kernelParams.RSDP = rsdp_request.response->address;
    g_kernelParams.kernelPhysical = kernel_address_request.response->physical_base;
    g_kernelParams.kernelVirtual = kernel_address_request.response->virtual_base;
    g_kernelParams.symbolTable = module_request.response->modules[0]->address;
    g_kernelParams.symbolTableSize = module_request.response->modules[0]->size;

    StartKernel();
    while (true) {}
}

} // extern "C"