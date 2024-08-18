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

#include <icxxabi.h>
#include <spinlock.h>

#ifdef __cplusplus
extern "C" {
#endif

atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];
uarch_t __atexit_func_count = 0;

void *__dso_handle = 0;

int __cxa_atexit(cxa_destructor_t func, void *arg, void *dso) {
    if (__atexit_func_count >= ATEXIT_MAX_FUNCS) {
        return -1;
    }

    __atexit_funcs[__atexit_func_count].dtor = func;
    __atexit_funcs[__atexit_func_count].obj_ptr = arg;
    __atexit_funcs[__atexit_func_count].dso_handle = dso;
    __atexit_func_count++;
    return 0;
}

void __cxa_finalize(void* f) {
    if (f == nullptr) {
        for (uarch_t i = __atexit_func_count; i > 0; i--) {
            if (__atexit_funcs[i - 1].dtor != nullptr) {
                (*__atexit_funcs[i - 1].dtor)(__atexit_funcs[i - 1].obj_ptr);
                __atexit_funcs[i - 1].dtor = nullptr;
            }
        }
    }
    else {
        for (uarch_t i = __atexit_func_count; i > 0; i--) {
            if (__atexit_funcs[i - 1].dtor == (cxa_destructor_t)f) {
                (*__atexit_funcs[i - 1].dtor)(__atexit_funcs[i - 1].obj_ptr);
                __atexit_funcs[i - 1].dtor = nullptr;
                // Now we move every other element one step up
                for (uarch_t j = i; j < __atexit_func_count; j++) {
                    __atexit_funcs[j - 1].dtor = __atexit_funcs[j].dtor;
                    __atexit_funcs[j - 1].obj_ptr = __atexit_funcs[j].obj_ptr;
                    __atexit_funcs[j - 1].dso_handle = __atexit_funcs[j].dso_handle;
                }
            }
        }
    }
}

void __cxa_pure_virtual() {

}

#ifdef __cplusplus
}
#endif

namespace __cxxabiv1 {
    typedef long int __guard;

    extern "C" int __cxa_guard_acquire(__guard* g) {
        return __atomic_test_and_set(g, __ATOMIC_ACQUIRE);
    }

    extern "C" void __cxa_guard_release(__guard* g) {
        __atomic_clear(g, __ATOMIC_RELEASE);
    }

    extern "C" void __cxa_guard_abort(__guard*) {

    }
}