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

add_compile_options(--sysroot=${FROSTYOS_SYSROOT})
add_link_options(--sysroot=${FROSTYOS_SYSROOT})

include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/kernel/headers/frostyos)

set(CMAKE_SKIP_INSTALL_RPATH ON)

# frostyos_library(<LibName> SOURCES <files...> [LIBS <libs...>] [PRIVATE_LIBS <libs...>])
function(frostyos_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS;PRIVATE_LIBS" ${ARGN})

    add_library(${name} SHARED ${ARG_SOURCES})

    string(REGEX REPLACE "^Lib" "" _output_name ${name})
    set_target_properties(${name} PROPERTIES OUTPUT_NAME ${_output_name})

    target_include_directories(${name}
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:data/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    if(ARG_LIBS)
        target_link_libraries(${name} PUBLIC ${ARG_LIBS})
    endif()
    if(ARG_PRIVATE_LIBS)
        target_link_libraries(${name} PRIVATE ${ARG_PRIVATE_LIBS})
    endif()

    target_compile_options(${name}
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C,ASM_NASM>:-g>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wall>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wextra>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wpedantic>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fstack-protector>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-O2>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wl,v>
    )

    target_link_options(${name}
        PRIVATE -v
    )

    install(TARGETS ${name}
        LIBRARY DESTINATION data/lib
        ARCHIVE DESTINATION data/lib)

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include)
        install(DIRECTORY include/ DESTINATION data/include)
    endif()

    set_property(GLOBAL APPEND PROPERTY FROSTYOS_USERLAND_TARGETS ${name})
endfunction()

# frostyos_program(<name> SOURCES <files...> [LIBS <libs...>])
function(frostyos_program name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    if(ARG_LIBS)
        target_link_libraries(${name} PRIVATE ${ARG_LIBS})
    endif()

    target_compile_options(${name}
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C,ASM_NASM>:-g>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wall>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wextra>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-Wpedantic>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-fstack-protector>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX,C>:-O2>
    )

    install(TARGETS ${name} RUNTIME DESTINATION data/bin)
    set_property(GLOBAL APPEND PROPERTY FROSTYOS_USERLAND_TARGETS ${name})
endfunction()

add_subdirectory(BasicPrograms)
add_subdirectory(Libraries)
add_subdirectory(ServiceManager)
add_subdirectory(WindowManager)