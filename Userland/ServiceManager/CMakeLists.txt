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

set(service_manager_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Init.cpp
)

frostyos_program(ServiceManager ${service_manager_sources})

install(CODE "
    set(_bin \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/data/bin\")
    file(MAKE_DIRECTORY \"\${_bin}\")
    file(REMOVE \"\${_bin}/init\")
    file(CREATE_LINK ServiceManager \"\${_bin}/init\" SYMBOLIC)
")
