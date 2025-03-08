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

#ifndef _BITMAP_HPP
#define _BITMAP_HPP

#include <stdint.h>

// A simple bitmap class where the data buffer needs to be provided by the user.
class RawBitmap {
public:
    RawBitmap();
    RawBitmap(uint8_t* buffer, uint64_t size); // size is in bytes

    void Set(uint64_t index, bool value);
    bool Get(uint64_t index) const;

    uint8_t* GetBuffer() const;
    uint64_t GetSize() const; // in bytes

    void SetBuffer(uint8_t* buffer);
    void SetSize(uint64_t size); // in bytes

private:
    uint8_t* m_Buffer;
    uint64_t m_Size; // in bytes
};

class [[gnu::packed]] RawPackedBitmap : public RawBitmap {
public:
    RawPackedBitmap() : RawBitmap() {}
    RawPackedBitmap(uint8_t* buffer, uint64_t size) : RawBitmap(buffer, size) {} // size is in bytes
};

#endif /* _BITMAP_HPP */