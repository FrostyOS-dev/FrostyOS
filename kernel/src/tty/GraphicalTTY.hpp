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

#ifndef _TTY_GRAPHICAL_HPP
#define _TTY_GRAPHICAL_HPP

#include "FBConsole.hpp"
#include "TTY.hpp"

#include <Graphics/Colour.hpp>

#define ANSI_MAX_PARAMS 16

class GraphicalTTY : public TTY {
public:
    GraphicalTTY();
    GraphicalTTY(FBConsole* console);
    ~GraphicalTTY() override;
    
    void SetConsole(FBConsole* console);
    FBConsole* GetConsole();

protected:
    void PutChars(const char* s, size_t n) override;
    void FlushHW() override;
    uint64_t GetColumn() override;
    int GetSize(winsize_t*) override;
    int SetSize(const winsize_t*) override;
    void ResetOutputState() override;

private:
    enum class EscState : uint8_t {
        Ground,
        Escape,
        CSI,
        OSC,
        OSCEscape,
        Charset
    };

    struct EscapeState {
        EscState state;
        int params[ANSI_MAX_PARAMS];
        int paramCount;
        int current;
        bool hasCurrent;
        bool ignore;
        char privateMarker;
        char intermediate;
    } m_escapeState;

    struct TextAttributes {
        Colour fg;
        Colour bg;
        int fgBase;
        bool bold;
        bool reverse;
    } m_attr;

    void ResetEscapeState();
    void ResetAttributes();
    void ApplyAttributes();
    void ProcessChar(char c);
    void PushParam();
    void ExecuteCSI(char final);
    void HandleSGR();
    void SoftReset();

    uint64_t m_savedX;
    uint64_t m_savedY;
    FBConsole* m_console;
};

#endif /* _TTY_GRAPHICAL_HPP */