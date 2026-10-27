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

script_dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$script_dir/.." && pwd)

fail() { echo "error: $1" >&2; exit 1; }

[ "$(id -u)" != 0 ] || fail "jinx refuses to run as root"

need="cmake ninja nasm xorriso rsync unshare flock wget"
[ "$1" = run-qemu ] && need="$need qemu-system-x86_64"
for t in $need; do
    command -v "$t" >/dev/null 2>&1 || fail "missing host tool: $t"
done

unshare --user --map-root-user true 2>/dev/null \
    || fail "unprivileged user namespaces are disabled (jinx needs them)"

cmake -S "$root/superbuild" -B "$root/build" -G Ninja ${FROSTYOS_BUILD_CONFIG:+-DFROSTYOS_BUILD_CONFIG="$FROSTYOS_BUILD_CONFIG"}
cmake --build "$root/build" ${1:+--target "$1"}

[ -e "$root/compile_commands.json" ] || [ -L "$root/compile_commands.json" ] \
    || ln -s build/os/compile_commands.json "$root/compile_commands.json"
