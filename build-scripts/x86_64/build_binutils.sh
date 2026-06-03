#!/bin/sh

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

# Exit on error
set -e

mkdir -p $TOOLCHAIN_PREFIX

BINUTILS_VERSION=2.45.1

# Check if x86_64-frostyos binutils exists and is version 2.45.1 using x86_64-frostyos-ld
if [ -f "$TOOLCHAIN_PREFIX/bin/x86_64-frostyos-ld" ]; then
    if [ "$($TOOLCHAIN_PREFIX/bin/x86_64-frostyos-ld -v | grep $BINUTILS_VERSION)" ]; then
        echo "x86_64-frostyos binutils is up to date."
        exit 0
    fi
fi

# Install x86_64-frostyos binutils

echo -----------------
echo Building binutils
echo -----------------
mkdir -p toolchain/binutils/{src,build}
cd toolchain/binutils/src
curl -OL https://ftpmirror.gnu.org/binutils/binutils-$BINUTILS_VERSION.tar.xz
tar -xf binutils-$BINUTILS_VERSION.tar.xz
rm binutils-$BINUTILS_VERSION.tar.xz
cd binutils-$BINUTILS_VERSION
patch -p1 < ../../../../patches/binutils.patch
cd ../../build
../src/binutils-$BINUTILS_VERSION/configure --target=x86_64-frostyos --prefix="$TOOLCHAIN_PREFIX" --with-sysroot=$SYSROOT --disable-nls --disable-werror --enable-shared --disable-gdb
make -j$(nproc)
make install
cd ../../..
rm -fr toolchain/binutils
