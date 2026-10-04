/*
Copyright (©) 2026  Frosty515

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

#ifndef _TTY_BACKEND_FBCON_HPP
#define _TTY_BACKEND_FBCON_HPP

#include "../FBConsole.hpp"
#include "../TTYBackend.hpp"

class TTYBackendFBConsole : public TTYBackend {
public:
    TTYBackendFBConsole();
    TTYBackendFBConsole(FBConsole* console);

    void Init(FBConsole* console);

    void WriteChar(char c) override;

    void SetCursor(uint64_t x, uint64_t y) override;
    void GetCursor(uint64_t& x, uint64_t& y) override;

    void Seek(uint64_t pos) override;

    void Flush() override;

    void SetConsole(FBConsole* console);

private:
    FBConsole* m_console;
};

#endif /* _TTY_BACKEND_FBCON_HPP */