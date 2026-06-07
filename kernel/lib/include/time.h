/*
Copyright (©) 2026  Frosty515

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

#ifndef _TIME_H
#define _TIME_H

typedef long time_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

/* POSIX extensions */

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_BOOTTIME 7

#endif /* _TIME_H */