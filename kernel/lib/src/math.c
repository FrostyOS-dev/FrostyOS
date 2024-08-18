/*
Copyright (©) 2022-2023  Frosty515

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
*/

#include "math.h"

int max(int a, int b) {
    return a > b ? a : b;
}

unsigned umax(unsigned a, unsigned b) {
    return a > b ? a : b;
}

long lmax(long a, long b) {
    return a > b ? a : b;
}

unsigned long ulmax(unsigned long a, unsigned long b) {
    return a > b ? a : b;
}

int min(int a, int b) {
    return a < b ? a : b;
}

unsigned umin(unsigned a, unsigned b) {
    return a < b ? a : b;
}

long lmin(long a, long b) {
    return a < b ? a : b;
}

unsigned long ulmin(unsigned long a, unsigned long b) {
    return a < b ? a : b;
}

int abs(int n) {
    return n < 0 ? (n * -1) : n;
}

unsigned uabs(unsigned n) {
    return n;
}

long labs(long n) {
    return n < 0 ? (n * -1) : n;
}

unsigned long ulabs(unsigned long n) {
    return n;
}
