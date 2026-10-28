# Copyright (©) 2024-2026  Frosty515

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

cmake_minimum_required(VERSION 3.26)
project(FrostyOS C CXX ASM_NASM)

include(${CMAKE_SOURCE_DIR}/build-scripts/utils.cmake)

set(FROSTYOS_BOOT ${CMAKE_BINARY_DIR}/dist/boot)
set(FROSTYOS_STAGE ${CMAKE_BINARY_DIR}/stage)
set(FROSTYOS_ROOTFS ${CMAKE_BINARY_DIR}/rootfs)
set(FROSTYOS_ISO ${CMAKE_BINARY_DIR}/iso/boot.iso)
set(CMAKE_INSTALL_PREFIX ${FROSTYOS_STAGE} CACHE PATH "" FORCE)

file (MAKE_DIRECTORY ${FROSTYOS_BOOT}/FrostyOS ${FROSTYOS_BOOT}/EFI/BOOT)

configure_file(${CMAKE_SOURCE_DIR}/build-scripts/${FROSTYOS_ARCHITECTURE}/limine.conf ${FROSTYOS_BOOT}/limine.conf)

# Limine binaries
add_custom_command(
	OUTPUT ${FROSTYOS_BOOT}/EFI/BOOT/BOOTX64.EFI ${FROSTYOS_BOOT}/limine-uefi-cd.bin
	COMMAND ${CMAKE_COMMAND} -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/build-scripts/download_boot_files.cmake
	COMMAND ${CMAKE_COMMAND} -E tar xJ ${CMAKE_BINARY_DIR}/limine-binary.tar.xz
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/limine-binary/BOOTX64.EFI ${FROSTYOS_BOOT}/EFI/BOOT/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/limine-binary/limine-uefi-cd.bin ${FROSTYOS_BOOT}/
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/limine-binary
	VERBATIM)
add_custom_target(limine_boot DEPENDS ${FROSTYOS_BOOT}/EFI/BOOT/BOOTX64.EFI ${FROSTYOS_BOOT}/limine-uefi-cd.bin)

# Limine include
add_custom_command(
	OUTPUT ${CMAKE_SOURCE_DIR}/kernel/src/limine.h
	COMMAND ${CMAKE_COMMAND} -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/build-scripts/download_limine_include.cmake
	VERBATIM)
add_custom_target(limine_include DEPENDS ${CMAKE_SOURCE_DIR}/kernel/src/limine.h)
add_custom_target(limine_include_update
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/kernel/src/limine.h
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target limine_include)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(kernel)
add_subdirectory(Userland)

get_property(_userland GLOBAL PROPERTY FROSTYOS_USERLAND_TARGETS)
add_custom_target(stage COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${FROSTYOS_STAGE})
if(_userland)
	add_dependencies(stage ${_userland})
endif()

add_custom_target(image ALL
	COMMAND ${CMAKE_SOURCE_DIR}/build-scripts/make_image.sh ${FROSTYOS_BOOT} ${FROSTYOS_ISO} ${FROSTYOS_ROOTFS}
			${FROSTYOS_SYSROOT} ${CMAKE_SOURCE_DIR}/Userland/jinx-sources/base-files ${FROSTYOS_STAGE}
	USES_TERMINAL VERBATIM)
add_dependencies(image install_kernel limine_boot stage)

set(_ovmf_dirs /usr/share/edk2/x64 /usr/share/edk2/ovmf /usr/share/OVMF /usr/share/qemu)

find_file(OVMF_CODE
	NAMES OVMF_CODE.4m.fd OVMF_CODE_4M.fd OVMF_CODE.fd
	PATHS ${_ovmf_dirs}
	NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

find_file(OVMF_VARS_TEMPLATE
	NAMES OVMF_VARS.4m.fd OVMF_VARS_4M.fd OVMF_VARS.fd
	PATHS ${_ovmf_dirs}
	NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

if(NOT OVMF_CODE OR NOT OVMF_VARS_TEMPLATE)
	message(FATAL_ERROR "OVMF firmware not found. Install edk2-ovmf, or pass -DOVMF_CODE=<path> -DOVMF_VARS_TEMPLATE=<path>")
endif()

set(OVMF_VARS ${CMAKE_BINARY_DIR}/OVMF_VARS.fd)
add_custom_command(OUTPUT ${OVMF_VARS} COMMAND ${CMAKE_COMMAND} -E copy ${OVMF_VARS_TEMPLATE} ${OVMF_VARS})
add_custom_target(ovmf_vars DEPENDS ${OVMF_VARS})

execute_process(COMMAND test -w /dev/kvm RESULT_VARIABLE _kvm OUTPUT_QUIET ERROR_QUIET)
if(_kvm EQUAL 0)
	set(_machine -M q35,accel=kvm -cpu host)
else()
	set(_machine -M q35 -cpu max) # TCG fallback
endif()

set(QEMU_ARGS
	-drive if=pflash,file=${OVMF_CODE},format=raw,readonly=on
	-drive if=pflash,file=${OVMF_VARS},format=raw
	-drive format=raw,file=${FROSTYOS_ISO},index=0,media=disk
	-m 1G ${_machine}
	-chardev pty,id=serial0 -serial chardev:serial0)
if(NOT FROSTYOS_BUILD_CONFIG STREQUAL "Release")
	list(APPEND QEMU_ARGS -debugcon file:/dev/stdout -monitor stdio)
endif()

add_custom_target(run-qemu COMMAND qemu-system-x86_64 ${QEMU_ARGS} USES_TERMINAL VERBATIM)
add_dependencies(run-qemu image ovmf_vars)
