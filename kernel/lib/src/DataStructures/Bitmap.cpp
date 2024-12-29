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

#include <DataStructures/Bitmap.hpp>

RawBitmap::RawBitmap() : m_Buffer(nullptr), m_Size(0) {

}

RawBitmap::RawBitmap(uint8_t* buffer, uint64_t size) : m_Buffer(buffer), m_Size(size) {

}

void RawBitmap::Set(uint64_t index, bool value) {
    if (index >= m_Size * 8)
        return;

    if (value)
        m_Buffer[index / 8] |= (1 << (index % 8));
    else
        m_Buffer[index / 8] &= ~(1 << (index % 8));
}

bool RawBitmap::Get(uint64_t index) const {
    if (index >= m_Size * 8)
        return false;

    return m_Buffer[index / 8] & (1 << (index % 8));
}

uint8_t* RawBitmap::GetBuffer() const {
    return m_Buffer;
}

uint64_t RawBitmap::GetSize() const {
    return m_Size;
}

void RawBitmap::SetBuffer(uint8_t* buffer) {
    m_Buffer = buffer;
}

void RawBitmap::SetSize(uint64_t size) {
    m_Size = size;
}
