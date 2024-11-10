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

cmake_minimum_required(VERSION 3.20)

project(FrostyOS C CXX ASM_NASM)

include(${CMAKE_SOURCE_DIR}/build-scripts/utils.cmake)

add_custom_target(build_boot
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT
	COMMAND ${CMAKE_COMMAND} -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/build-scripts/download_boot_files.cmake
)

add_dependencies(build_boot install_kernel)

if (UNIX)
	find_program(DD dd)
	if (NOT DD)
		message(FATAL_ERROR "Missing required tool: dd")
	endif()
	find_program(MFORMAT mformat)
	if (NOT MFORMAT)
		message(FATAL_ERROR "Missing required tool: mformat")
	endif()
	find_program(MMD mmd)
	if (NOT MMD)
		message(FATAL_ERROR "Missing required tool: mmd")
	endif()
	find_program(MCOPY mcopy)
	if (NOT MCOPY)
		message(FATAL_ERROR "Missing required tool: mcopy")
	endif()

	add_custom_target(build_mkgpt
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} sh build-scripts/build_mkgpt.sh
		BYPRODUCTS ${CMAKE_SOURCE_DIR}/depend/tools/bin/mkgpt
	)

	add_custom_target(update_ovmf
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} sh build-scripts/update_ovmf.sh
		BYPRODUCTS ${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.fd
	)

	add_custom_target(build_iso
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso
		COMMAND dd if=/dev/zero of=${CMAKE_SOURCE_DIR}/iso/fat.img bs=1k count=10240
		COMMAND mformat -i ${CMAKE_SOURCE_DIR}/iso/fat.img -h 4 -t 80 -s 64 -I 32 ::
		COMMAND mmd -i ${CMAKE_SOURCE_DIR}/iso/fat.img ::/EFI
		COMMAND mmd -i ${CMAKE_SOURCE_DIR}/iso/fat.img ::/EFI/BOOT
		COMMAND mcopy -i ${CMAKE_SOURCE_DIR}/iso/fat.img ${CMAKE_SOURCE_DIR}/dist/boot/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT
		COMMAND mmd -i ${CMAKE_SOURCE_DIR}/iso/fat.img ::/FrostyOS
		COMMAND mcopy -i ${CMAKE_SOURCE_DIR}/iso/fat.img ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS/kernel.elf ::/FrostyOS
		COMMAND mcopy -i ${CMAKE_SOURCE_DIR}/iso/fat.img ${CMAKE_SOURCE_DIR}/dist/boot/FrostyOS/ksymbols.map ::/FrostyOS
		COMMAND mcopy -i ${CMAKE_SOURCE_DIR}/iso/fat.img ${CMAKE_SOURCE_DIR}/dist/boot/limine.cfg ::
		COMMAND ${CMAKE_SOURCE_DIR}/depend/tools/bin/mkgpt -o ${CMAKE_SOURCE_DIR}/iso/hdimage.bin --image-size 24576 --part ${CMAKE_SOURCE_DIR}/iso/fat.img --type system
		BYPRODUCTS ${CMAKE_SOURCE_DIR}/iso/hdimage.bin ${CMAKE_SOURCE_DIR}/iso/fat.img
	)
endif()

add_dependencies(build_iso build_boot build_mkgpt update_ovmf)

if(FROSTYOS_BUILD_CONFIG STREQUAL "Debug")
	add_custom_target(run-qemu
		COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/hdimage.bin,index=0,media=disk -m 24G -debugcon stdio  -M q35 -cpu qemu64 -no-reboot -no-shutdown -s -smp 2 # -machine accel=kvm
		USES_TERMINAL
	)
else()
	if(FROSTYOS_BUILD_CONFIG STREQUAL "Release")
		add_custom_target(run-qemu
			COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/hdimage.bin,index=0,media=disk -m 256M -machine accel=kvm -M q35 -cpu qemu64
			USES_TERMINAL
		)
	else() # Default is Debug
		add_custom_target(run-qemu
			COMMAND qemu-system-x86_64 -drive if=pflash,file=/usr/share/edk2/x64/OVMF_CODE.fd,format=raw,readonly=on -drive if=pflash,file=${CMAKE_SOURCE_DIR}/ovmf/x86-64/OVMF_VARS.fd,format=raw -drive format=raw,file=${CMAKE_SOURCE_DIR}/iso/hdimage.bin,index=0,media=disk -m 256M -debugcon stdio -machine accel=kvm -M q35 -cpu qemu64
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
