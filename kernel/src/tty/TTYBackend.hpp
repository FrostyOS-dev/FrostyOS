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

#ifndef _TTY_BACKEND_HPP
#define _TTY_BACKEND_HPP

#include <stdint.h>
#include <spinlock.h>

enum class TTYBackendType {
    VGA,
    Debug,
    INVALID
};

enum class TTYBackendStream {
    IN,
    OUT,
    ERR,
    DEBUG,
    INVALID
};

class TTYBackend {
public:
    TTYBackend();
    TTYBackend(TTYBackendType type);

    virtual void WriteChar(char c);
    virtual void WriteString(const char* str);

    virtual char ReadChar();
    virtual void ReadString(char* str, uint64_t length);

    virtual void SetCursor(uint64_t x, uint64_t y);
    virtual void GetCursor(uint64_t& x, uint64_t& y);

    virtual void Seek(uint64_t pos);

    TTYBackendType GetType() const;

    void Lock() const;
    void Unlock() const;

private:
    TTYBackendType m_type;
    mutable spinlock_t m_lock;
};

#endif /* _TTY_BACKEND_HPP */