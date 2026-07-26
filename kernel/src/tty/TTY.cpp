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

#include "TTY.hpp"
#include "TTYBackend.hpp"

TTY* g_CurrentTTY = nullptr;

TTY::TTY() : m_backends{nullptr, nullptr, nullptr, nullptr}, m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED) {
    
}

void TTY::Init() {
    
}

void TTY::WriteChar(char c, TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->WriteChar(c);
        if (m_debugMirroring && (stream == TTYStream::OUT || stream == TTYStream::ERR))
            m_backends[(uint64_t)TTYStream::DEBUG]->WriteChar(c);
    }
}

void TTY::WriteString(const char* str, TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->WriteString(str);
        if (m_debugMirroring && (stream == TTYStream::OUT || stream == TTYStream::ERR))
            m_backends[(uint64_t)TTYStream::DEBUG]->WriteString(str);
    }
}

void TTY::WriteString(const char* str, uint64_t length, TTYStream stream, bool flush) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->WriteString(str, length, flush);
        if (m_debugMirroring && (stream == TTYStream::OUT || stream == TTYStream::ERR))
            m_backends[(uint64_t)TTYStream::DEBUG]->WriteString(str, length, flush);
    }
}

char TTY::ReadChar(TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        return m_backends[(uint64_t)stream]->ReadChar();
    }
    return '\0';
}

void TTY::ReadString(char* str, uint64_t length, TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->ReadString(str, length);
    }
}

void TTY::SetCursor(uint64_t x, uint64_t y, TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->SetCursor(x, y);
    }
}

void TTY::GetCursor(uint64_t& x, uint64_t& y, TTYStream stream) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->GetCursor(x, y);
    }
}

void TTY::SetBackend(TTYBackend* backend, TTYStream stream) {
    m_backends[(uint64_t)stream] = backend;
}

TTYBackend* TTY::GetBackend(TTYStream stream) const {
    return m_backends[(uint64_t)stream];
}

void TTY::Seek(TTYStream stream, uint64_t pos) {
    if (m_backends[(uint64_t)stream] != nullptr) {
        m_backends[(uint64_t)stream]->Seek(pos);
    }
}

void TTY::Lock(TTYStream stream) const {
    if (m_backends[(uint64_t)stream] != nullptr)
        m_backends[(uint64_t)stream]->Lock();
}

void TTY::Unlock(TTYStream stream) const {
    if (m_backends[(uint64_t)stream] != nullptr)
        m_backends[(uint64_t)stream]->Unlock();
}

void TTY::ForceUnlockAll() const {
    for (int i = 0; i < 4; i++) {
        if (m_backends[i] != nullptr)
            m_backends[i]->ForceUnlock();
    }
}

void TTY::EnableDebugMirroring() {
    m_debugMirroring = true;
}

void TTY::DisableDebugMirroring() {
    m_debugMirroring = false;
}

bool TTY::IsDebugMirroring() const {
    return m_debugMirroring;
}
