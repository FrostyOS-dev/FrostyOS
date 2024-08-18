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

include(${ROOT_SOURCE_DIR}/build-scripts/utils.cmake)

download_file("https://raw.githubusercontent.com/limine-bootloader/limine/v8.x-binary/BOOTX64.EFI" ${ROOT_SOURCE_DIR}/dist/boot/EFI/BOOT/BOOTX64.EFI)
download_file("https://raw.githubusercontent.com/FrostyOS-dev/various-scripts/master/FrostyOS/boot/limine.cfg" ${ROOT_SOURCE_DIR}/dist/boot/limine.cfg)
