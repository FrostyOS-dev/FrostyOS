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

#include <assert.h>
#include <new.hpp>
#include <stdlib.h>

void* operator new(size_t size) {
    void* mem = kcalloc(1, size);
    assert(mem != nullptr);
    return mem;
}

void* operator new[](size_t size) {
    void* mem = kcalloc(1, size);
    assert(mem != nullptr);
    return mem;
}

void operator delete(void* p) {
    kfree(p);
}

void operator delete(void* p, size_t size) {
    (void)size;
    kfree(p);
}

void operator delete[](void* p) {
    kfree(p);
}

void operator delete[](void* p, size_t size) {
    (void)size;
    kfree(p);
}
