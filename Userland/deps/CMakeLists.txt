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

add_custom_target(deps)
add_dependencies(deps mlibc)

add_custom_target(mlibc
    COMMAND ${CMAKE_COMMAND} -E env DESTDIR=${SYSROOT} ninja -C ${CMAKE_CURRENT_BINARY_DIR}/mlibc_build install
    USES_TERMINAL
)

add_custom_target(mlibc_headers
    COMMAND ${CMAKE_COMMAND} -E env DESTDIR=${SYSROOT} ninja -C ${CMAKE_CURRENT_BINARY_DIR}/mlibc_headers_build install > /dev/null
)

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/mlibc_headers_build)
    add_custom_target(mlibc_headers_configure
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} meson setup --cross-file=${CMAKE_CURRENT_SOURCE_DIR}/frostyos.cross-file --prefix=/data -Dheaders_only=true mlibc_headers_build ${CMAKE_CURRENT_SOURCE_DIR}/mlibc
        USES_TERMINAL
    )

    add_dependencies(mlibc_headers mlibc_headers_configure)
endif ()

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/mlibc_build)
    add_custom_target(mlibc_configure
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} meson setup --cross-file=${CMAKE_CURRENT_SOURCE_DIR}/frostyos.cross-file --prefix=/data -Ddefault_library=static -Ddefault_library_paths=/data/lib -Dno_headers=true mlibc_build ${CMAKE_CURRENT_SOURCE_DIR}/mlibc
        USES_TERMINAL
    )

    add_dependencies(mlibc mlibc_configure mlibc_headers)
else ()
    add_dependencies(mlibc mlibc_headers)
endif ()
