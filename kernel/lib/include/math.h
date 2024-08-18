/*
Copyright (©) 2022-2024  Frosty515

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

#ifndef _KERNEL_MATH_H
#define _KERNEL_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
    unsigned quot;
    unsigned rem;
} udiv_t;

typedef struct {
  long quot;
  long rem;
} ldiv_t;

typedef struct {
  unsigned long quot;
  unsigned long rem;
} uldiv_t;

int max(int a, int b);
unsigned umax(unsigned a, unsigned b);
long lmax(long a, long b);
unsigned long ulmax(unsigned long a, unsigned long b);

int min(int a, int b);
unsigned umin(unsigned a, unsigned b);
long lmin(long a, long b);
unsigned long ulmin(unsigned long a, unsigned long b);

int abs(int n);
unsigned uabs(unsigned n);
long labs(long n);
unsigned long ulabs(unsigned long n);


/* Implemented in architecture-specific assembly */

div_t div(int numer, int denom);
udiv_t udiv(unsigned numer, unsigned denom);
ldiv_t ldiv(long numer, long denom);
uldiv_t uldiv(unsigned long numer, unsigned long denom);

unsigned char log2(unsigned long int num);

unsigned char get_lsb(unsigned long int num); // get the lowest set bit

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_MATH_H */