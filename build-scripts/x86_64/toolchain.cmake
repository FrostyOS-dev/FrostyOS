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

set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR x86-64)


set(CMAKE_C_COMPILER x86_64-frostyos-gcc)
set(CMAKE_CXX_COMPILER x86_64-frostyos-g++)
set(CMAKE_ASM-ATT_COMPILER x86_64-frostyos-as)
set(CMAKE_ASM_NASM_COMPILER nasm)
set(CMAKE_C_COMPILER_WORKS true)
set(CMAKE_CXX_COMPILER_WORKS true)
#set(CMAKE_ASM-ATT_COMPILER_WORKS true)
#set(CMAKE_ASM_NASM_COMPILER_WORKS true)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

find_program(HAS_x86_64_frostyos_nm x86_64-frostyos-nm)
if(NOT HAS_x86_64_frostyos_nm)
    message(FATAL_ERROR "Could not find x86_64-frostyos-nm")
else()
    set(NM x86_64-frostyos-nm)
endif()

find_program(HAS_x86_64_frostyos_ar x86_64-frostyos-ar)
if(NOT HAS_x86_64_frostyos_ar)
    message(FATAL_ERROR "Could not find x86_64-frostyos-ar")
else()
    set(AR x86_64-frostyos-ar)
endif()

set(FROSTYOS_ARCHITECTURE x86_64)
set(FROSTYOS_BUILD_TARGET kernel)

set(SYSROOT ${CMAKE_SOURCE_DIR}/root)

set(FROSTYOS_INSTALL_PREFIX ${SYSROOT}/data)

set(CMAKE_CXX_FLAGS "-fno-exceptions -fno-rtti")
set(CMAKE_C_FLAGS "")


