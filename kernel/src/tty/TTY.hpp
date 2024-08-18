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

#ifndef _TTY_HPP
#define _TTY_HPP

#include <stdint.h>

#include "TTYBackend.hpp"

class TTY {
public:
    TTY();

    void Init();

    void WriteChar(char c, TTYBackendStream stream = TTYBackendStream::OUT);
    void WriteString(const char* str, TTYBackendStream stream = TTYBackendStream::OUT);

    char ReadChar(TTYBackendStream stream = TTYBackendStream::IN);
    void ReadString(char* str, uint64_t length, TTYBackendStream stream = TTYBackendStream::IN);

    void SetCursor(uint64_t x, uint64_t y, TTYBackendStream stream = TTYBackendStream::OUT);
    void GetCursor(uint64_t& x, uint64_t& y, TTYBackendStream stream = TTYBackendStream::OUT);

    void SetBackend(TTYBackend* backend, TTYBackendStream stream);
    TTYBackend* GetBackend(TTYBackendStream stream) const;

    void Seek(TTYBackendStream stream, uint64_t pos);

private:
    TTYBackend* m_backends[4];
};

extern TTY* g_CurrentTTY;

#endif /* _TTY_HPP */