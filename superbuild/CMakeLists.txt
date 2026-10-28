# Copyright (©) 2026  Frosty515

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
project(FrostyOS-superbuild NONE)
include(ExternalProject)

set(FROSTYOS_ARCHITECTURE x86_64)
set(FROSTYOS_BUILD_CONFIG Debug CACHE STRING "Debug or Release")

get_filename_component(REPO ${CMAKE_SOURCE_DIR}/.. ABSOLUTE)
set(JS         ${REPO}/Userland/jinx-sources)
set(JINX       ${JS}/jinx/jinx)
set(JINX_BUILD ${REPO}/.jinx-build)
set(TOOLCHAIN  ${CMAKE_BINARY_DIR}/toolchain)
set(SYSROOT    ${CMAKE_BINARY_DIR}/sysroot)
set(STAMPS     ${CMAKE_BINARY_DIR}/stamps)
set(HOST_TOOLS ${CMAKE_BINARY_DIR}/host-tools)
set(SCRIPTS    ${REPO}/build-scripts)
file(MAKE_DIRECTORY ${JINX_BUILD} ${SYSROOT} ${STAMPS})

# add_stage(<name> WATCH <paths...> [DEPENDS <targets...>] RUN <shell command>)
function(add_stage name)
    cmake_parse_arguments(ARG "" "RUN" "WATCH;DEPENDS" ${ARGN})
    add_custom_target(${name}
        COMMAND ${SCRIPTS}/run_if_changed.sh ${STAMPS}/${name} ${ARG_WATCH} -- sh -c "${ARG_RUN}"
        WORKING_DIRECTORY ${JINX_BUILD}
        USES_TERMINAL VERBATIM)
    if(ARG_DEPENDS)
        add_dependencies(${name} ${ARG_DEPENDS})
    endif()
endfunction()

# host tools
ExternalProject_Add(host_tools
    SOURCE_DIR ${REPO}/tools
    BINARY_DIR ${CMAKE_BINARY_DIR}/host-tools-build
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${HOST_TOOLS} -DFROSTYOS_BUILD_CONFIG=${FROSTYOS_BUILD_CONFIG}
    BUILD_ALWAYS ON)

# jinx
add_custom_command(OUTPUT ${JINX_BUILD}/.jinx-parameters
    COMMAND ${JINX} init ${JS} ARCH=${FROSTYOS_ARCHITECTURE}
    WORKING_DIRECTORY ${JINX_BUILD} VERBATIM)

add_custom_target(jinx_init DEPENDS ${JINX_BUILD}/.jinx-parameters)

# kernel headers
set(KHDR ${REPO}/kernel/headers/frostyos)
add_stage(kernel_headers_for_mlibc
    WATCH ${KHDR}
    RUN "${CMAKE_COMMAND} -E copy_directory ${KHDR}/asm ${JS}/deps/mlibc/sysdeps/frostyos/include/asm")

# Stage 1: cross binutils + gcc + libstdc++
add_stage(toolchain
    DEPENDS jinx_init kernel_headers_for_mlibc
    WATCH ${JS}/Jinxfile ${JS}/host-recipes ${JS}/recipes/mlibc ${JS}/recipes/mlibc-headers
    RUN "${JINX} update -b host:libstdc++-v3 && ${SCRIPTS}/export_toolchain.sh ${JS}/.jinx-cache/xbps/bin ${JINX_BUILD}/host-pkgs ${TOOLCHAIN}")

# Stage 2: mlibc
add_stage(mlibc
    DEPENDS toolchain
    WATCH ${JS}/deps/mlibc ${JS}/deps/frostyos.cross-file ${JS}/recipes/mlibc ${JS}/recipes/mlibc-headers ${KHDR}
    RUN "${JINX} build mlibc-headers && ${JINX} install -f ${SYSROOT} mlibc-headers && ${JINX} build mlibc && ${JINX} install -f ${SYSROOT} mlibc")

# Stage 3: ported software
set(PORTS bash binutils coreutils gcc)
list(JOIN PORTS " " PORTS_STR)
add_stage(ports
    DEPENDS mlibc
    WATCH ${JS}/recipes ${JS}/Jinxfile
    RUN "${JINX} update -b ${PORTS_STR} && ${JINX} install -f ${SYSROOT} ${PORTS_STR}")

ExternalProject_Add(os
    SOURCE_DIR ${REPO}
    BINARY_DIR ${CMAKE_BINARY_DIR}/os
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${REPO}/build-scripts/${FROSTYOS_ARCHITECTURE}/toolchain.cmake
        -DFROSTYOS_TOOLCHAIN_ROOT=${TOOLCHAIN}/usr/local
        -DFROSTYOS_SYSROOT=${SYSROOT}
        -DFROSTYOS_HOST_TOOLS=${HOST_TOOLS}
        -DFROSTYOS_BUILD_CONFIG=${FROSTYOS_BUILD_CONFIG}
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    DEPENDS ports host_tools
    BUILD_ALWAYS ON
    CONFIGURE_HANDLED_BY_BUILD ON
    INSTALL_COMMAND ""
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD ON)

add_custom_target(run-qemu
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/os --target run-qemu
    USES_TERMINAL)
add_dependencies(run-qemu os)

add_custom_target(image
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/os --target image
    USES_TERMINAL)
add_dependencies(image os)
