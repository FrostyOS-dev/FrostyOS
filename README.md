# FrostyOS

## COPYING

Copyright (©) 2022-2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

## Status

This is the 3rd iteration of FrostyOS.

## Building and running

### Prerequisites

This OS is developed on Linux, and is expected to be built and run on Linux. It may be possible to build and run it on other operating systems, but this is not guaranteed and may require additional work. The following tools are required to build and run the OS:

- A C/C++ compiler that supports C++23 and C23 (note: MSVC is not supported, see note 1 below)
- nasm
- CMake 3.26 or newer
- Ninja
- bash (see note 2 below)
- xorriso
- curl
- tar
- other unix utilities (e.g. rm, mkdir, etc.)
- QEMU
- OVMF
- Other tools that may be required to run jinx.

#### Notes

1. By default, GCC is requested and used as the compiler, but modifying the `CMakeLists.txt` file in the `tools` folder to use a different compiler should be possible. However, MSVC is not supported.
2. A posix-compliant shell is enough, but some scripts may require bash-specific features, or call bash itself, so it's recommended to have bash installed.

### Building

- To build the OS, run `./build-scripts/build.sh` from the root of the repository. This will build the OS and its tools, and create a bootable image. This script will download and build required dependencies where possible, and will give an error for any that are missing. If the OS is intented to be run immediately after building, it is recommend to follow the instructions in the "Running" section below, which will also build the OS if it has been modified since the last build.

### Running

- To run the OS in QEMU, run `./build-scripts/run.sh` from the root of the repository. This will start QEMU with the appropriate settings to run the OS. This will also rebuild the OS if it has been modified since the last build.
