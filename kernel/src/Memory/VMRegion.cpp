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

#include "VMRegion.hpp"

VMRegion::VMRegion(void* start, void* end, VMRegionFlags flags) : m_start(start), m_end(end), m_flags(flags) {

}

VMRegion::~VMRegion() {

}

void* VMRegion::GetStart() const {
    return m_start;
}

void* VMRegion::GetEnd() const {
    return m_end;
}

size_t VMRegion::GetSize() const {
    return (size_t)m_end - (size_t)m_start;
}

VMRegionFlags VMRegion::GetFlags() const {
    return m_flags;
}

void VMRegion::SetFlags(VMRegionFlags flags) {
    m_flags = flags; // does not get remapped. That is the responsibility of whatever is holding this instance.
}

void VMRegion::ExpandLeft(size_t size) {
    m_start = (void*)((size_t)m_start - size);
}

void VMRegion::ExpandRight(size_t size) {
    m_end = (void*)((size_t)m_end + size);
}

bool VMRegion::Contains(void* addr, size_t size) const {
    if (size == 0)
        return addr >= m_start && addr < m_end;
    else
        return addr >= m_start && (size_t)addr + size <= (size_t)m_end;
}

bool VMRegion::Overlaps(const VMRegion& other) const {
    return !((other.m_start < m_start && other.m_end <= m_start) || (m_start < other.m_start && m_end <= other.m_start));
}
