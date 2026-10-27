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

set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR x86-64)

set(FROSTYOS_TOOLCHAIN_ROOT "" CACHE PATH "exported toolchain (…/usr/local)")
set(FROSTYOS_SYSROOT "" CACHE PATH "jinx sysroot")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES FROSTYOS_TOOLCHAIN_ROOT FROSTYOS_SYSROOT)

set(_tc ${FROSTYOS_TOOLCHAIN_ROOT}/bin/x86_64-frostyos-)
set(CMAKE_C_COMPILER ${_tc}gcc)
set(CMAKE_CXX_COMPILER ${_tc}g++)
set(CMAKE_AR ${_tc}gcc-ar)
set(CMAKE_RANLIB ${_tc}gcc-ranlib)

set(CMAKE_C_COMPILER_FORCED true)
set(CMAKE_CXX_COMPILER_FORCED true)
set(CMAKE_ASM_NASM_COMPILER nasm)
set(CMAKE_ASM_NASM_COMPILER_FORCED true)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")

set(CMAKE_FIND_ROOT_PATH ${FROSTYOS_SYSROOT} ${CMAKE_BINARY_DIR}/stage)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(FROSTYOS_ARCHITECTURE x86_64)
set(FROSTYOS_BUILD_TARGET kernel)
set(CMAKE_CXX_FLAGS "-fno-exceptions -fno-rtti")