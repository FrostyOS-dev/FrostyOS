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

# Update OVMF firmware variables

# Check if ovmf/x86-64/OVMF_VARS.fd exists
if [ -f "ovmf/x86-64/OVMF_VARS.fd" ]; then
    exit 0
fi

# Copy OVMF firmware variables from /usr/share/edk2/ovmf/OVMF_VARS.fd

mkdir -p ovmf/x86-64
cp /usr/share/edk2/x64/OVMF_VARS.fd ovmf/x86-64