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

# Check if x86_64-frostyos binutils exists and is version 2.42 using x86_64-frostyos-ld
if [ -f "$TOOLCHAIN_PREFIX/bin/x86_64-frostyos-ld" ]; then
    if [ "$($TOOLCHAIN_PREFIX/bin/x86_64-frostyos-ld -v | grep '2.42')" ]; then
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
git clone https://github.com/FrostyOS-dev/binutils-gdb.git --depth 1 --branch binutils-2_42-branch binutils-2.42
cd ../build
../src/binutils-2.42/configure --target=x86_64-frostyos --prefix="$TOOLCHAIN_PREFIX" --with-sysroot=$SYSROOT --disable-nls --disable-werror --enable-shared --disable-gdb
make -j$(nproc)
make install
cd ../../..
rm -fr toolchain/binutils
