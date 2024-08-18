/*
Copyright (©) 2024  Frosty515

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

#ifndef _ICXXABI_H
#define _ICXXABI_H

#define ATEXIT_MAX_FUNCS 128

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cxa_destructor_t)(void*);
typedef void (*cxa_atexit_t)(void);
typedef unsigned int uarch_t;

struct atexit_func_entry_t {
    cxa_destructor_t dtor;
    void* obj_ptr;
    void* dso_handle;
};

int __cxa_atexit(cxa_destructor_t dtor, void* obj, void* dso);
void __cxa_finalize(void* f);

#ifdef __cplusplus
}
#endif

#endif