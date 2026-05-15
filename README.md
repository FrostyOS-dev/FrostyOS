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
- CMake 3.20 or newer
- Ninja
- bash (see note 2 below)
- dd
- dosfstools (e.g. mformat, mmd, mcopy)
- curl
- tar
- other unix utilities (e.g. rm, mkdir, etc.)
- QEMU
- OVMF
- make and other tools that may be required for building GCC and binutils (see the GCC and binutils documentation for details)

#### Notes

1. By default, GCC is requested and used as the compiler, but modifying the `CMakeLists.txt` file in the `tools` folder to use a different compiler should be possible. However, MSVC is not supported.
2. A posix-compliant shell is enough, but some scripts may require bash-specific features, or call bash itself, so it's recommended to have bash installed.

### Building

- To enter the build environment, run `./build-scripts/enter-environment.sh` from the root of the repository. This will start a new shell with the environment variables set up for building the OS and its tools.
- To build the OS, run `./build-scripts/build.sh` from the root of the repository. This will build the OS and its tools, and create a bootable image. This script will also build GCC and binutils if they are not already built, and will download the required sources for them if they are not already downloaded. This script **must** be run from within the build environment on the first run, and is recommended to be run from within the build environment on subsequent runs as well, as it will ensure that the environment variables are set up correctly. If the OS is intented to be run immediately after building, it is recommend to follow the instructions in the "Running" section below, which will also build the OS if it has been modified since the last build.

### Running

- To run the OS in QEMU, run `./build-scripts/run.sh` from the root of the repository. This will start QEMU with the appropriate settings to run the OS. This will also rebuild the OS if it has been modified since the last build. As with the build script, this script is recommended to be run from within the build environment, as it will ensure that the environment variables are set up correctly.