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

cmake_minimum_required(VERSION 3.20)

project(FrostyOS C CXX ASM_NASM)

include(${CMAKE_SOURCE_DIR}/build-scripts/utils.cmake)

add_custom_target(build_boot
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT
	COMMAND ${CMAKE_COMMAND} -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/build-scripts/download_boot_files.cmake
	COMMAND ${CMAKE_COMMAND} -E tar xJ ${CMAKE_BINARY_DIR}/limine-binary.tar.xz
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/limine-binary/BOOTX64.EFI ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/limine-binary/limine-uefi-cd.bin ${CMAKE_SOURCE_DIR}/dist/boot/
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/limine-binary
	BYPRODUCTS ${CMAKE_BINARY_DIR}/limine-binary.tar.xz ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT/BOOTX64.EFI ${CMAKE_SOURCE_DIR}/dist/boot/limine-uefi-cd.bin ${CMAKE_SOURCE_DIR}/dist/boot/limine.conf ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT ${CMAKE_SOURCE_DIR}/dist/boot/EFI
)

add_dependencies(build_boot install_kernel)

add_custom_target(build_initramfs
	COMMAND ${CMAKE_COMMAND} -E chdir ${SYSROOT} /bin/sh -c "tar -c --no-auto-compress --owner=0 --group=0 -f ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS/initramfs.tar *"
	BYPRODUCTS ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS/initramfs.tar
)

add_dependencies(build_initramfs install_kernel Userland)

if (UNIX)
	add_custom_target(update_ovmf
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} sh build-scripts/update_ovmf.sh
		BYPRODUCTS ${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.fd
	)

	add_custom_target(build_iso
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso
		COMMAND xorriso -report_about SORRY -as mkisofs -quiet --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label ${CMAKE_SOURCE_DIR}/dist/boot -o ${CMAKE_SOURCE_DIR}/iso/boot.iso
		BYPRODUCTS ${CMAKE_SOURCE_DIR}/iso/boot.iso ${CMAKE_SOURCE_DIR}/iso
	)
endif()

add_dependencies(build_iso build_boot build_initramfs update_ovmf)

if(FROSTYOS_BUILD_CONFIG STREQUAL "Debug")
	add_custom_target(run-qemu
		COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.4m.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/boot.iso,index=0,media=disk -m 256M -debugcon file:/dev/stdout -monitor stdio -machine accel=kvm -M q35 -cpu host -s
		USES_TERMINAL
	)
else()
	if(FROSTYOS_BUILD_CONFIG STREQUAL "Release")
		add_custom_target(run-qemu
			COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.4m.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/boot.iso,index=0,media=disk -m 256M -machine accel=kvm -M q35 -cpu qemu64
			USES_TERMINAL
		)
	else() # Default is Debug
		add_custom_target(run-qemu
			COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.4m.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/boot.iso,index=0,media=disk -m 256M -debugcon file:/dev/stdout -monitor stdio -machine accel=kvm -M q35 -cpu qemu64
			USES_TERMINAL
		)
	endif()
endif()

add_dependencies(run-qemu build_iso)

add_custom_target(limine_include
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/kernel/src/limine.h
	COMMAND ${CMAKE_COMMAND} -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/build-scripts/download_limine_include.cmake
)

add_subdirectory(kernel)
add_subdirectory(Userland)
