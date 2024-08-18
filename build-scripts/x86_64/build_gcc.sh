#!/bin/sh

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

mkdir -p $TOOLCHAIN_PREFIX

# Now check that x86_64-frostyos-gcc is version 14.1.1

if [ -f "$TOOLCHAIN_PREFIX/bin/x86_64-frostyos-gcc" ]; then
    if [ "$($TOOLCHAIN_PREFIX/bin/x86_64-frostyos-gcc -dumpversion | grep '14.1.1')" ]; then
        echo "x86_64-frostyos GCC is up to date."
        exit 0
    fi
fi

# Install kernel and LibC headers
mkdir -p $SYSROOT/data/include/kernel
cp -r kernel/headers/* $SYSROOT/data/include/kernel/
cp -r Userland/Libraries/LibC/include/* $SYSROOT/data/include/

# Install x86_64-frostyos-gcc

echo ------------
echo Building GCC
echo ------------
mkdir -p toolchain/gcc/{src,build}
cd toolchain/gcc/src
git clone https://github.com/FrostyOS-dev/gcc.git --depth 1 --branch releases/gcc-14 gcc-14.1.1
cd ../build
../src/gcc-14.1.1/configure --target=x86_64-frostyos --prefix="$TOOLCHAIN_PREFIX" --with-sysroot=$SYSROOT --disable-nls --enable-shared --enable-languages=c,c++
make -j$(nproc) all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ../../..
rm -fr toolchain/gcc
