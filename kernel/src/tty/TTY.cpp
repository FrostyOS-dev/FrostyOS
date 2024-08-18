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

#include "TTY.hpp"

TTY* g_CurrentTTY = nullptr;

TTY::TTY() : m_backends{nullptr, nullptr, nullptr, nullptr} {
    
}

void TTY::Init() {
    
}

void TTY::WriteChar(char c, TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->WriteChar(c);
    }
}

void TTY::WriteString(const char* str, TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->WriteString(str);
    }
}

char TTY::ReadChar(TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        return m_backends[(uint64_t)stream]->ReadChar();
    }
    return '\0';
}

void TTY::ReadString(char* str, uint64_t length, TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->ReadString(str, length);
    }
}

void TTY::SetCursor(uint64_t x, uint64_t y, TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->SetCursor(x, y);
    }
}

void TTY::GetCursor(uint64_t& x, uint64_t& y, TTYBackendStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->GetCursor(x, y);
    }
}

void TTY::SetBackend(TTYBackend* backend, TTYBackendStream stream) {
    m_backends[(uint64_t)stream] = backend;
}

TTYBackend* TTY::GetBackend(TTYBackendStream stream) const {
    return m_backends[(uint64_t)stream];
}

void TTY::Seek(TTYBackendStream stream, uint64_t pos) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->Seek(pos);
    }
}
