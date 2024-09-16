# Copyright (©) 2024  Frosty515

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

if(FROSTYOS_ARCHITECTURE STREQUAL "x86_64")
    set(kernel_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/APIC/IPI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/APIC/LAPIC.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/IDT.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/IDT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/ISR.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/ISR.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/interrupts/PIC.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PageTables.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PageTables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PagingInit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PagingUtil.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PagingUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PAT.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Memory/PAT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/ArchDefs.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/E9.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/GDT.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/GDT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/IO.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/KernelSymbols.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/MSR.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Panic.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Panic.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Processor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/Stack.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/start.asm
    )

    set(kernel_lib_sources
        ${kernel_lib_sources}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/arch/x86_64/math.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/arch/x86_64/spinlock.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/arch/x86_64/util.asm
    )
endif()

set(kernel_sources
    ${kernel_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Data-structures/Bitmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Colour.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Framebuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VGA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VGAFont.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/ACPI/Init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/ACPI/MADT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/ACPI/SDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/ACPI/XSDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/HAL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/PageFault.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/PagingUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/PMM.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sanitiser/sanitiser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sanitiser/ubsan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tty/backends/DebugBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tty/backends/VGABackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tty/TTY.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tty/TTYBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/limine_entry.cpp
)

set(kernel_lib_sources
    ${kernel_lib_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/assert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/icxxabi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/stack_protector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/stdio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/stdlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src/util.c
)

add_executable(kernel ${kernel_sources} ${kernel_lib_sources})

set_target_properties(kernel PROPERTIES CXX_STANDARD 23)
set_target_properties(kernel PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(kernel PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(kernel PROPERTIES C_STANDARD 23)
set_target_properties(kernel PROPERTIES C_EXTENSIONS OFF)
set_target_properties(kernel PROPERTIES C_STANDARD_REQUIRED ON)

target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/headers)
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld)

target_link_options(kernel
    PUBLIC -T
    PUBLIC ${LINKER_SCRIPT}
    PUBLIC -static
    PUBLIC -Bsymbolic
    PUBLIC -nostdlib
    PUBLIC -zmax-page-size=0x1000
    PRIVATE -mno-red-zone
)

target_compile_definitions(kernel PRIVATE _FROSTYOS_BUILD_TARGET=kernel)

target_compile_options(kernel
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-mcmodel=kernel>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-g>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wall>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wextra>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wpedantic>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-strict-aliasing>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-pointer-arith>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unused-parameter>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-switch>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-packed-bitfield-compat>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-nostdlib>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-ffreestanding>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fno-pie>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fstack-protector>
    # PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fsanitize=undefined>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-mgeneral-regs-only>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-mno-red-zone>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-O2>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fno-omit-frame-pointer>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

set_target_properties(kernel PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

target_link_libraries(kernel PRIVATE gcc)

target_compile_definitions(kernel PRIVATE _IS_IN_KERNEL=1)

if (FROSTYOS_BUILD_TARGET STREQUAL "kernel")

    add_custom_target(create_dist_dir ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS
    )

    add_dependencies(kernel limine_include create_dist_dir)

    add_custom_target(install_kernel
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS
        COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:kernel> -o ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS/kernel.elf
    )

endif()