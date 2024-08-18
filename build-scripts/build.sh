#!/bin/bash

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

# Exit on error
set -e

# build the host system utils
# ./build-scripts/build_utils.sh

# We have to check if the toolchain is up to date before building the OS
# First we check if the $TOOLCHAIN_PREFIX is set, if not set it to toolchain/local
if [ -z "$TOOLCHAIN_PREFIX" ]; then
    TOOLCHAIN_PREFIX=$PWD/toolchain/local
fi

# Check if $SYSROOT is set, if not set it to root
if [ -z "$SYSROOT" ]; then
    SYSROOT=$PWD/root
fi

# Make the toolchain directory prefix, and install kernel and LibC headers
mkdir -p $TOOLCHAIN_PREFIX

export TOOLCHAIN_PREFIX SYSROOT

./build-scripts/build_toolchain.sh

# Check if $FROSTYOS_BUILD_CONFIG is set, if not set it to Debug
if [ -z "$FROSTYOS_BUILD_CONFIG" ]; then
    FROSTYOS_BUILD_CONFIG=Debug
fi

# Check if build directory exists
if [ -d "build" ]; then
    cd build
else
    mkdir build
    cd build
fi

cmake -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DFROSTYOS_BUILD_CONFIG=$FROSTYOS_BUILD_CONFIG --toolchain=build-scripts/x86_64/toolchain.cmake ..
ninja build_iso
