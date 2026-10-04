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

#include "FBConsoleBackend.hpp"

#include "../FBConsole.hpp"
#include "../TTYBackend.hpp"

#include <math.h>

#include <Graphics/VGAFont.hpp>

TTYBackendFBConsole::TTYBackendFBConsole() : TTYBackend(TTYBackendType::FBConsole), m_console(nullptr) {

}

TTYBackendFBConsole::TTYBackendFBConsole(FBConsole* console) : TTYBackend(TTYBackendType::FBConsole), m_console(console) {

}

void TTYBackendFBConsole::Init(FBConsole* console) {
    m_console = console;
}

void TTYBackendFBConsole::WriteChar(char c) {
    if (m_console != nullptr)
        m_console->PrintChar(c);
}

void TTYBackendFBConsole::SetCursor(uint64_t x, uint64_t y) {
    if (m_console != nullptr)
        m_console->SetCursor(x * CHAR_WIDTH, y * CHAR_HEIGHT);
}

void TTYBackendFBConsole::GetCursor(uint64_t& x, uint64_t& y) {
    if (m_console != nullptr) {
        uint64_t realX, realY;
        m_console->GetCursor(realX, realY);
        x = realX / CHAR_WIDTH;
        y = realY / CHAR_HEIGHT;
    }
}

void TTYBackendFBConsole::Seek(uint64_t pos) {
    if (m_console != nullptr) {
        uldiv_t div = uldiv(pos, m_console->GetNumberOfColumns());
        m_console->SetCursor(div.rem * CHAR_WIDTH, div.quot * CHAR_HEIGHT);
    }
}

void TTYBackendFBConsole::Flush() {
    m_console->Flush();
}

void TTYBackendFBConsole::SetConsole(FBConsole* console) {
    m_console = console;
}
