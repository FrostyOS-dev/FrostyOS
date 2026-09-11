/*
Copyright (©) 2024-2026  Frosty515

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

#include <new.hpp>
#include <stdlib.h>

// TODO: respect alignment

void* operator new(size_t size) {
    return kcalloc(1, size);
}

void* operator new(size_t size, std::align_val_t alignment) {
    (void)alignment;
    return kcalloc(1, size);
}

void* operator new[](size_t size) {
    return kcalloc(1, size);
}

void* operator new[](size_t size, std::align_val_t alignment) {
    (void)alignment;
    return kcalloc(1, size);
}

void operator delete(void* p) {
    kfree(p);
}

void operator delete(void* ptr, std::align_val_t alignment) noexcept {
    (void)alignment;
    kfree(ptr);
}

void operator delete(void* p, size_t size) {
    (void)size;
    kfree(p);
}

void operator delete(void* ptr, size_t size, std::align_val_t alignment) noexcept {
    (void)size;
    (void)alignment;
    kfree(ptr);
}

void operator delete[](void* p) {
    kfree(p);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept {
    (void)alignment;
    kfree(ptr);
}

void operator delete[](void* p, size_t size) {
    (void)size;
    kfree(p);
}

void operator delete[](void* ptr, size_t size, std::align_val_t alignment) noexcept {
    (void)size;
    (void)alignment;
    kfree(ptr);
}
