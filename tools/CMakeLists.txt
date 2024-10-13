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

project(FrostyOS-host-tools CXX C ASM_NASM)

find_program(NASM nasm)
if(NOT NASM)
    message(FATAL_ERROR "nasm not found")
endif()

find_program(GCC gcc)
if(NOT GCC)
    message(FATAL_ERROR "gcc not found")
endif()

find_program(GXX g++)
if(NOT GXX)
    message(FATAL_ERROR "g++ not found")
endif()

set(CMAKE_C_COMPILER ${GCC})
set(CMAKE_CXX_COMPILER ${GXX})
set(CMAKE_ASM_NASM_COMPILER ${NASM})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

add_executable(buildsymboltable src/buildsymboltable.cpp)

target_compile_options(buildsymboltable
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-g>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wall>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wextra>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-O2>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

set_target_properties(buildsymboltable PROPERTIES CXX_STANDARD 23)
set_target_properties(buildsymboltable PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(buildsymboltable PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(buildsymboltable PROPERTIES C_STANDARD 23)
set_target_properties(buildsymboltable PROPERTIES C_STANDARD_REQUIRED ON)
set_target_properties(buildsymboltable PROPERTIES C_EXTENSIONS OFF)

install(TARGETS buildsymboltable DESTINATION ${CMAKE_SOURCE_DIR}/bin)