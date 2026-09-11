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

#ifndef _NEW_HPP
#define _NEW_HPP

#include <stddef.h>

namespace std {
    enum class align_val_t : size_t {};
}

void* operator new(size_t size);
void* operator new(size_t size, std::align_val_t alignment);

void* operator new[](size_t size);
void* operator new[](size_t size, std::align_val_t alignment);

void operator delete(void* p);
void operator delete(void* ptr, std::align_val_t alignment) noexcept;

void operator delete(void* p, size_t size);
void operator delete(void* ptr, size_t size, std::align_val_t alignment) noexcept;

void operator delete[](void* p);
void operator delete[](void* ptr, std::align_val_t alignment) noexcept;

void operator delete[](void* p, size_t size);
void operator delete[](void* ptr, size_t size, std::align_val_t alignment) noexcept;

#endif /* _NEW_HPP */