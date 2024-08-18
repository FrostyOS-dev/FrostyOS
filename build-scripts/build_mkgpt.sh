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

# Check if depend/tools/bin/mkgpt exists
if [ -f "depend/tools/bin/mkgpt" ]; then
    echo "mkgpt is already installed."
    exit 0
fi

# Install mkgpt
rm -fr depend/tools/build/mkgpt
mkdir -p depend/tools/build/mkgpt
curl -OL https://github.com/FrostyOS-dev/various-scripts/raw/master/mkgpt/mkgpt.tar.gz
tar -xf mkgpt.tar.gz -C depend/tools/build/mkgpt
rm -fr mkgpt.tar.gz
cd depend/tools/build/mkgpt
./configure
make -j$(nproc)
cd ../..
mkdir -p bin licenses/mkgpt
curl -o licenses/mkgpt/LICENSE https://raw.githubusercontent.com/FrostyOS-dev/various-scripts/master/mkgpt/LICENSE
cp build/mkgpt/mkgpt bin/
chmod 755 bin/mkgpt
rm -fr build/mkgpt