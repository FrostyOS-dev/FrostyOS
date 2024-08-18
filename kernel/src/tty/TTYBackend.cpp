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

#include "TTYBackend.hpp"

TTYBackend::TTYBackend() : m_type(TTYBackendType::INVALID) {

}

TTYBackend::TTYBackend(TTYBackendType type) : m_type(type) {
    
}

void TTYBackend::WriteChar(char c) {
    
}

void TTYBackend::WriteString(const char* str) {
    
}

char TTYBackend::ReadChar() {
    return '\0';
}

void TTYBackend::ReadString(char* str, uint64_t length) {
    
}

void TTYBackend::SetCursor(uint64_t x, uint64_t y) {
    
}

void TTYBackend::GetCursor(uint64_t& x, uint64_t& y) {
    
}

void TTYBackend::Seek(uint64_t pos) {
    
}

TTYBackendType TTYBackend::GetType() const {
    return m_type;
}
