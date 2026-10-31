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

#include "FBConsole.hpp"
#include "GraphicalTTY.hpp"

#include <debug.h>

#include <errno.h>
#include <stdint.h>
#include <util.h>

#include <Graphics/VGAFont.hpp>

// Standard VGA-style Linux Console Colors
#define ANSI_COLOR_BLACK   Colour(0x00, 0x00, 0x00)
#define ANSI_COLOR_RED     Colour(0xAA, 0x00, 0x00)
#define ANSI_COLOR_GREEN   Colour(0x00, 0xAA, 0x00)
#define ANSI_COLOR_YELLOW  Colour(0xAA, 0x55, 0x00)
#define ANSI_COLOR_BLUE    Colour(0x00, 0x00, 0xAA)
#define ANSI_COLOR_MAGENTA Colour(0xAA, 0x00, 0xAA)
#define ANSI_COLOR_CYAN    Colour(0x00, 0xAA, 0xAA)
#define ANSI_COLOR_WHITE   Colour(0xAA, 0xAA, 0xAA)
#define ANSI_COLOR_DEFAULT_FG Colour(0xFF, 0xFF, 0xFF)
#define ANSI_COLOR_DEFAULT_BG Colour(0x00, 0x00, 0x00)

static Colour& GetANSI256Colour(uint8_t index) {
    static Colour s_ansiColours[256];
    static bool s_initialized = false;

    if (!s_initialized) {
        // Standard 16 colors (0-15)
        s_ansiColours[0] = Colour(0x00, 0x00, 0x00);
        s_ansiColours[1] = Colour(0xAA, 0x00, 0x00);
        s_ansiColours[2] = Colour(0x00, 0xAA, 0x00);
        s_ansiColours[3] = Colour(0xAA, 0x55, 0x00);
        s_ansiColours[4] = Colour(0x00, 0x00, 0xAA);
        s_ansiColours[5] = Colour(0xAA, 0x00, 0xAA);
        s_ansiColours[6] = Colour(0x00, 0xAA, 0xAA);
        s_ansiColours[7] = Colour(0xAA, 0xAA, 0xAA);
        
        // Bright equivalents (8-15)
        s_ansiColours[8] = Colour(85, 85, 85);
        s_ansiColours[9] = Colour(255, 85, 85);
        s_ansiColours[10] = Colour(85, 255, 85);
        s_ansiColours[11] = Colour(255, 255, 85);
        s_ansiColours[12] = Colour(85, 85, 255);
        s_ansiColours[13] = Colour(255, 85, 255);
        s_ansiColours[14] = Colour(85, 255, 255);
        s_ansiColours[15] = Colour(255, 255, 255);

        // 6x6x6 color cube (16-231)
        for (int i = 16; i <= 231; i++) {
            int cubeIndex = i - 16;
            uint8_t r = (cubeIndex / 36) > 0 ? 55 + (cubeIndex / 36) * 40 : 0;
            uint8_t g = ((cubeIndex / 6) % 6) > 0 ? 55 + ((cubeIndex / 6) % 6) * 40 : 0;
            uint8_t b = (cubeIndex % 6) > 0 ? 55 + (cubeIndex % 6) * 40 : 0;
            s_ansiColours[i] = Colour(r, g, b);
        }

        // 24-step Grayscale (232-255)
        for (int i = 232; i <= 255; i++) {
            uint8_t gray = 8 + (i - 232) * 10;
            s_ansiColours[i] = Colour(gray, gray, gray);
        }

        s_initialized = true;
    }

    return s_ansiColours[index];
}

static inline int CSIParam(const int* params, int count, int index, int def) {
    return (index < count && params[index] > 0) ? params[index] : def;
}

static inline uint8_t ClampColour(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : static_cast<uint8_t>(v));
}

GraphicalTTY::GraphicalTTY() : TTY(TTYType::Graphical), m_savedX(0), m_savedY(0), m_console(nullptr) {
    ResetEscapeState();
    ResetAttributes();
}

GraphicalTTY::GraphicalTTY(FBConsole* console) : TTY(TTYType::Graphical), m_savedX(0), m_savedY(0), m_console(console) {
    ResetEscapeState();
    ResetAttributes();
}

GraphicalTTY::~GraphicalTTY() {

}

void GraphicalTTY::SetConsole(FBConsole* console) {
    m_console = console;
}

FBConsole* GraphicalTTY::GetConsole() {
    return m_console;
}

void GraphicalTTY::PutChars(const char* s, size_t n) {
    if (m_console == nullptr)
        return;

    for (uint64_t i = 0; i < n; i++) {
        char c = s[i];

        if (m_debugMirroring)
            debug_putc(s[i]);

        ProcessChar(c);
    }
}

void GraphicalTTY::FlushHW() {
    if (m_console == nullptr)
        return;

    m_console->Flush();
}

uint64_t GraphicalTTY::GetColumn() {
    if (m_console == nullptr)
        return 0;

    uint64_t x, y;
    m_console->GetCursor(x, y);
    return x / CHAR_WIDTH;
}

int GraphicalTTY::GetSize(winsize_t* size) {
    if (m_console == nullptr)
        return -ENODEV;
    size->ws_col = m_console->GetNumberOfColumns();
    size->ws_row = m_console->GetNumberOfRows();
    size->ws_xpixel = m_console->GetWidth();
    size->ws_ypixel = m_console->GetHeight();
    return ESUCCESS;
}

int GraphicalTTY::SetSize(const winsize_t* size) {
    if (m_console == nullptr)
        return -ENODEV;
    // Say it was successful if the requested dimensions are <= to the current, but don't actually set them.
    if (size->ws_col > m_console->GetNumberOfColumns() || size->ws_row > m_console->GetNumberOfRows())
        return -EINVAL;
    if (size->ws_xpixel > m_console->GetWidth() || size->ws_ypixel > m_console->GetHeight())
        return -EINVAL;
    return ESUCCESS;
}

void GraphicalTTY::ResetOutputState() {
    ResetEscapeState();
    SoftReset();
}

void GraphicalTTY::ResetEscapeState() {
    m_escapeState.state = EscState::Ground;
    for (int i = 0; i < ANSI_MAX_PARAMS; i++)
        m_escapeState.params[i] = 0;
    m_escapeState.paramCount = 0;
    m_escapeState.current = 0;
    m_escapeState.hasCurrent = 0;
    m_escapeState.ignore = 0;
    m_escapeState.privateMarker = 0;
    m_escapeState.intermediate = 0;
}

void GraphicalTTY::ResetAttributes() {
    m_attr.fg = ANSI_COLOR_DEFAULT_FG;
    m_attr.bg = ANSI_COLOR_DEFAULT_BG;
    m_attr.fgBase = -1;
    m_attr.bold = false;
    m_attr.reverse = false;
}

void GraphicalTTY::ApplyAttributes() {
    if (m_console == nullptr)
        return;

    Colour fg = m_attr.fg;
    if (m_attr.bold && m_attr.fgBase >= 0)
        fg = GetANSI256Colour(m_attr.fgBase + 8);
    Colour bg = m_attr.bg;

    if (m_attr.reverse) {
        Colour tmp = fg;
        fg = bg;
        bg = tmp;
    }

    m_console->SetForegroundColour(fg);
    m_console->SetBackgroundColour(bg);
}

void GraphicalTTY::ProcessChar(char c) {
    EscapeState& es = m_escapeState;

    if (es.state != EscState::Ground && (c == 0x18 || c == 0x1A)) {
        es.state = EscState::Ground;
        return;
    }

    switch (es.state) {
    case EscState::Ground:
        switch (c) {
        case '\x1b':
            es.state = EscState::Escape;
            break;
        case '\n':
        case '\v':
            m_console->LineFeed();
            break;
        case '\f':
            m_console->ClearScreen();
            m_console->SetCursor(0, 0);
            break;
        default:
            m_console->PrintChar(c);
            break;
        }
        break;

    case EscState::Escape:
        switch (c) {
        case '[':
            ResetEscapeState();
            es.state = EscState::CSI;
            break;
        case ']':
            es.current = 0;
            es.state = EscState::OSC;
            break;
        case '(':
        case ')':
        case '*':
        case '+':
        case '#':
        case '%':
            es.state = EscState::Charset;
            break;
        case '7':
            m_console->GetCursor(m_savedX, m_savedY);
            es.state = EscState::Ground;
            break;
        case '8':
            m_console->SetCursor(m_savedX, m_savedY);
            es.state = EscState::Ground;
            break;
        case 'c': // full reset
            SoftReset();
            m_console->EraseInDisplay(2);
            m_console->SetCursor(0, 0);
            es.state = EscState::Ground;
            break;
        case 'D':
            m_console->LineFeed();
            es.state= EscState::Ground;
            break;
        case 'E':
            m_console->NewLine();
            es.state = EscState::Ground;
            break;
        case 'M':
            m_console->ReverseLineFeed();
            es.state = EscState::Ground;
            break;
        case '\x1b': // double escape, maintain current state
            break;
        default:
            es.state = EscState::Ground;
            break;
        }
        break;

    case EscState::CSI:
        if (c >= '0' && c <= '9') {
            if (es.current < 100000)
                es.current = es.current * 10 + (c - '0');
            es.hasCurrent = true;
        } else if (c == ';')
            PushParam();
        else if (c >= '<' && c <= '?') {
            if (es.paramCount == 0 && !es.hasCurrent && es.privateMarker == 0)
                es.privateMarker = c;
            else
                es.ignore = true;
        } else if (c >= 0x20 && c <= 0x2F) {
            if (es.intermediate == 0)
                es.intermediate = c;
            else
                es.ignore = true;
        } else if (c == ':')
            es.ignore = true;
        else if (c >= 0x40 && c <= 0x7E) {
            if (es.hasCurrent || es.paramCount > 0)
                PushParam();
            if (!es.ignore)
                ExecuteCSI(c);
            es.state = EscState::Ground;
        } else if (c == '\x1b')
            es.state = EscState::Escape;
        break;

    case EscState::OSC:
        if (c == '\a')
            es.state = EscState::Ground;
        else if (c == '\x1b')
            es.state = EscState::OSCEscape;
        else if (++es.current > 256)
            es.state = EscState::Ground;
        break;

    case EscState::OSCEscape:
        es.state = EscState::Ground;
        if (c != '\\') {
            es.state = EscState::Escape;
            ProcessChar(c);
        }
        break;

    case EscState::Charset:
        es.state = EscState::Ground;
        break;
    }
}

void GraphicalTTY::PushParam() {
    EscapeState& es = m_escapeState;
    if (es.paramCount < ANSI_MAX_PARAMS)
        es.params[es.paramCount++] = es.current;
    es.current = 0;
    es.hasCurrent = false;
}

void GraphicalTTY::ExecuteCSI(char final) {
    EscapeState& es = m_escapeState;
    if (es.intermediate != 0) {
        if (es.intermediate == '!' && final == 'p' && es.privateMarker == 0)
            SoftReset();
        return;
    }

    if (es.privateMarker != 0) {
        if (es.privateMarker == '?' && (final == 'h' || final == 'l'))
            for (int i = 0; i < es.paramCount; i++)
                if (es.params[i] == 1)
                    SetApplicationCursorKeys(final == 'h');
        return;
    }

    const int* params = m_escapeState.params;
    int count = m_escapeState.paramCount;

    uint64_t rows = m_console->GetNumberOfRows();
    uint64_t cols = m_console->GetNumberOfColumns();
    if (rows == 0 || cols == 0)
        return;

    uint64_t x, y;
    m_console->GetCursor(x, y);
    uint64_t col = MIN(x / CHAR_WIDTH, cols - 1);
    uint64_t row = MIN(y / CHAR_HEIGHT, rows - 1);
    uint64_t n = CSIParam(params, count, 0, 1);
    int mode = (count > 0) ? params[0] : 0;
    bool move = true;

    switch (final) {
    case 'A': row = (n > row) ? 0 : row - n; break;
    case 'B':
    case 'e': row = MIN(row + n, rows - 1); break;
    case 'C':
    case 'a': col = MIN(col + n, cols - 1); break;
    case 'D': col = (n > col) ? 0 : col - n; break;
    case 'E': row = MIN(row + n, rows - 1); col = 0; break;
    case 'F': row = (n > row) ? 0 : row - n; col = 0; break;
    case 'G':
    case '`': col = MIN(n - 1, cols - 1); break;
    case 'd': row = MIN(n - 1, rows - 1); break;
    case 'H':
    case 'f':
        row = MIN(n - 1, rows - 1);
        col = MIN(static_cast<uint64_t>(CSIParam(params, count, 1, 1)) - 1, cols -1);
        break;
    case 's': m_console->GetCursor(m_savedX, m_savedY); move = false; break;
    case 'u': m_console->SetCursor(m_savedX, m_savedY); move = false; break;
    case 'J': m_console->EraseInDisplay(mode); move = false; break;
    case 'K': m_console->EraseInLine(mode); move = false; break;
    case 'm': HandleSGR(); move = false; break;
    case 'L': m_console->InsertLines(n); move = false; break;
    case 'M': m_console->DeleteLines(n); move = false; break;
    case '@': m_console->InsertChars(n); move = false; break;
    case 'P': m_console->DeleteChars(n); move = false; break;
    case 'X': m_console->EraseChars(n); move = false; break;
    case 'S': m_console->Scroll(n); move = false; break;
    case 'T': m_console->ScrollDown(n); move = false; break;
    case 'r':
        if (m_console->SetScrollRegion(n - 1, CSIParam(params, count, 1, static_cast<int>(rows)) - 1))
            m_console->SetCursor(0, 0);
        move = false;
        break;
    default: move = false; break;
    }

    if (move)
        m_console->SetCursor(col * CHAR_WIDTH, row * CHAR_HEIGHT);
}

void GraphicalTTY::HandleSGR() {
    const int* params = m_escapeState.params;
    int count = m_escapeState.paramCount;

    for (int i = 0; i < count || i == 0; i++) { // no params == SGR 0
        int cmd = (count == 0) ? 0 : params[i];

        switch (cmd) {
        case 0: ResetAttributes(); break;
        case 1: m_attr.bold = true; break;
        case 22: m_attr.bold = false; break;
        case 7: m_attr.reverse = true; break;
        case 27: m_attr.reverse = false; break;
        case 39: m_attr.fg = ANSI_COLOR_DEFAULT_FG; m_attr.fgBase = -1; break;
        case 49: m_attr.bg = ANSI_COLOR_DEFAULT_BG; break;

        case 38:
        case 48: {
            Colour colour;
            if (i + 2 < count && params[i + 1] == 5) {
                colour = GetANSI256Colour(ClampColour(params[i + 2]));
                i += 2;
            } else if (i + 4 < count && params[i + 1] == 2) {
                colour = Colour(ClampColour(params[i + 2]), ClampColour(params[i + 3]), ClampColour(params[i + 4]));
                i += 4;
            } else {
                i = count; // malformed, drop the rest
                break;
            }
            if (cmd == 38) {
                m_attr.fg = colour;
                m_attr.fgBase = -1;
            } else
                m_attr.bg = colour;
            break;
        }

        default:
            if (cmd >= 30 && cmd <= 37) {
                m_attr.fg = GetANSI256Colour(cmd - 30);
                m_attr.fgBase = cmd - 30;
            } else if (cmd >= 40 && cmd <= 47)
                m_attr.bg = GetANSI256Colour(cmd - 40);
            else if (cmd >= 90 && cmd <= 97) {
                m_attr.fg = GetANSI256Colour(cmd - 90 + 8);
                m_attr.fgBase = -1;
            } else if (cmd >= 100 && cmd <= 107)
                m_attr.bg = GetANSI256Colour(cmd - 100 + 8);
            // underline, italic, blink, etc. are ignored
            break;
        }
    }
    ApplyAttributes();
}

void GraphicalTTY::SoftReset() {
    ResetAttributes();
    ApplyAttributes();
    SetApplicationCursorKeys(false);
    m_console->SetScrollRegion(0, m_console->GetNumberOfRows() - 1);
}
