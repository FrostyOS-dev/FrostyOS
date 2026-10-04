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

set(JINX
    ${CMAKE_CURRENT_SOURCE_DIR}/jinx/jinx
)

set(PACKAGES
    bash
)

set(ALWAYS_BUILD_PACKAGES
    mlibc
)

# Define the primary target for building the packages
add_custom_target(jinx-sources
    COMMAND ${JINX} build ${ALWAYS_BUILD_PACKAGES}
    COMMAND ${JINX} update -b ${PACKAGES}
    COMMAND ${JINX} install ${SYSROOT} ${PACKAGES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    USES_TERMINAL
)

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/jinx_sysroot_prepared)
    add_custom_target(jinx-sources_configure
        COMMAND ${JINX} init ${CMAKE_CURRENT_SOURCE_DIR} ARCH=${FROSTYOS_ARCHITECTURE} TOOLCHAIN_DIR=${FROSTYOS_TOOLCHAIN_PREFIX}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/jinx_sysroot_prepared
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        USES_TERMINAL
    )
    
    add_dependencies(jinx-sources jinx-sources_configure install_kernel_headers)
else ()
    add_dependencies(jinx-sources install_kernel_headers)
endif ()

set_property(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/jinx_sysroot_prepared"
)