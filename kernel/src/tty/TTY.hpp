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

#ifndef _TTY_HPP
#define _TTY_HPP

#include <stdint.h>

#include "TTYBackend.hpp"

#define DEBUG_MIRRORING_DEFAULT_ENABLED true

class TTY {
public:
    TTY();

    void Init();

    void WriteChar(char c, TTYStream stream = TTYStream::OUT);
    void WriteString(const char* str, TTYStream stream = TTYStream::OUT);
    void WriteString(const char* str, uint64_t length, TTYStream stream = TTYStream::OUT, bool flush = false);

    char ReadChar(TTYStream stream = TTYStream::IN);
    void ReadString(char* str, uint64_t length, TTYStream stream = TTYStream::IN);

    void SetCursor(uint64_t x, uint64_t y, TTYStream stream = TTYStream::OUT);
    void GetCursor(uint64_t& x, uint64_t& y, TTYStream stream = TTYStream::OUT);

    void SetBackend(TTYBackend* backend, TTYStream stream);
    TTYBackend* GetBackend(TTYStream stream) const;

    void Seek(TTYStream stream, uint64_t pos);

    void Lock(TTYStream stream) const;
    void Unlock(TTYStream stream) const;
    void ForceUnlockAll() const;

    void EnableDebugMirroring();
    void DisableDebugMirroring();
    bool IsDebugMirroring() const;

private:
    TTYBackend* m_backends[4];
    bool m_debugMirroring;
};

extern TTY* g_CurrentTTY;

#endif /* _TTY_HPP */