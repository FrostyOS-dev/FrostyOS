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

#include <cstdint>
#include <errno.h>

#include <Graphics/VGAFont.hpp>

#define ANSI_MAX_PARAMS 16

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

TTY* g_CurrentTTY = nullptr;

TTY::TTY() : m_inputBackend(nullptr), m_outputBackend(nullptr), m_debugBackend(nullptr), m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_type(TTYType::Invalid) {
    
}

TTY::TTY(TTYType type) : m_inputBackend(nullptr), m_outputBackend(nullptr), m_debugBackend(nullptr), m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_type(type) {

}

TTY::~TTY() {

}

void TTY::Init() {
    
}

int TTY::Read(char* buf, size_t size, size_t* realCount) {
    if (m_inputBackend == nullptr)
        return -ENODEV;
    size_t i = 0;
    buf[i] = m_inputBackend->ReadChar();
    for (i = 1; i < size; i++) {
        char c = 0;
        bool rc = m_inputBackend->ReadCharNoBlock(&c);
        if (!rc)
            break;
        buf[i] = c;
    }
    *realCount = i;
    return 0;
}

int TTY::ReadBlock(char* buf, size_t size) {
    if (m_inputBackend == nullptr)
        return -ENODEV;
    for (size_t i = 0; i < size; i++)
        buf[i] = m_inputBackend->ReadChar();
    return 0;
}

int TTY::Write(const char* buf, size_t size, bool flush) {
    if (m_outputBackend == nullptr)
        return -ENODEV;
    for (size_t i = 0; i < size; i++) {
        m_outputBackend->WriteChar(buf[i]);
        if (m_debugMirroring && m_debugBackend != nullptr)
            m_debugBackend->WriteChar(buf[i]);
    }
    if (flush)
        m_outputBackend->Flush();
    return 0;
}

int TTY::WriteDebug(const char* buf, size_t size) {
    if (m_debugBackend == nullptr)
        return -ENODEV;
    for (size_t i = 0; i < size; i++)
        m_debugBackend->WriteChar(buf[i]);
    return 0;
}

void TTY::SetCursor(uint64_t x, uint64_t y) {
    m_outputBackend->SetCursor(x, y);
}

void TTY::GetCursor(uint64_t& x, uint64_t& y) {
    m_outputBackend->GetCursor(x, y);
}

void TTY::SetInputBackend(TTYBackend* backend) {
    m_inputBackend = backend;
}

void TTY::SetOutputBackend(TTYBackend* backend) {
    m_outputBackend = backend;
}

void TTY::SetDebugBackend(TTYBackend* backend) {
    m_debugBackend = backend;
}

TTYBackend* TTY::GetInputBackend() const {
    return m_inputBackend;
}

TTYBackend* TTY::GetOutputBackend() const {
    return m_outputBackend;
}

TTYBackend* TTY::GetDebugBackend() const {
    return m_debugBackend;
}

void TTY::Seek(uint64_t pos) {
    m_inputBackend->Seek(pos);
    m_outputBackend->Seek(pos);
    m_debugBackend->Seek(pos);
}

uint64_t TTY::GetMaxSeek() const {
    return UINT64_MAX; // unknown
}

uint64_t TTY::GetCurrentSeek() const {
    return UINT64_MAX; // unknown
}

void TTY::FlushOutput() {
    m_outputBackend->Flush();
}

void TTY::Lock(TTYStream stream) const {
    switch (stream) {
    case TTYStream::IN:
        m_inputBackend->Lock();
        break;
    case TTYStream::OUT:
        m_outputBackend->Lock();
        break;
    case TTYStream::DEBUG:
        m_debugBackend->Lock();
        break;
    default:
        break;
    }
}

void TTY::Unlock(TTYStream stream) const {
    switch (stream) {
    case TTYStream::IN:
        m_inputBackend->Unlock();
        break;
    case TTYStream::OUT:
        m_outputBackend->Unlock();
        break;
    case TTYStream::DEBUG:
        m_debugBackend->Unlock();
        break;
    default:
        break;
    }
}

void TTY::ForceUnlockAll() const {
    m_inputBackend->ForceUnlock();
    m_outputBackend->ForceUnlock();
    m_debugBackend->ForceUnlock();
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

TTYType TTY::GetType() const {
    return m_type;
}

void TTY::SetType(TTYType type) {
    m_type = type;
}

bool TTY::CanRead(TTYStream stream) {
    switch (stream) {
    case TTYStream::IN:
        return true;
    default:
        return false;
    }
}

bool TTY::CanWrite(TTYStream stream) {
    switch (stream) {
    case TTYStream::OUT:
    case TTYStream::DEBUG:
        return true;
    default:
        return false;
    }
}

void ParseANSIParams(const char* escapeStr, int* params, int& paramCount) {
    paramCount = 0;
    int currentVal = 0;
    bool hasVal = false;

    for (int i = 1; escapeStr[i] != '\0'; i++) {
        char c = escapeStr[i];
        if (c >= '0' && c <= '9') {
            currentVal = (currentVal * 10) + (c - '0');
            hasVal = true;
        } else if (c == ';') {
            if (paramCount < ANSI_MAX_PARAMS)
                params[paramCount++] = currentVal;
            currentVal = 0;
            hasVal = false;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            if (hasVal && paramCount < ANSI_MAX_PARAMS)
                params[paramCount++] = currentVal;
            break;
        }
    }
}

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

GraphicalTTY::GraphicalTTY() : TTY(TTYType::Graphical), m_video(nullptr) {

}

GraphicalTTY::GraphicalTTY(VideoDevice* video) : TTY(TTYType::Graphical), m_video(video) {

}

GraphicalTTY::~GraphicalTTY() {

}

void GraphicalTTY::Init() {

}

int GraphicalTTY::Write(const char* buf, size_t size, bool flush) {
    if (m_video == nullptr)
        return -ENODEV;

    for (uint64_t i = 0; i < size; i++) {
        char c = buf[i];

        if (m_debugMirroring && m_debugBackend != nullptr)
            m_debugBackend->WriteChar(c);

        if (m_escapeState.inEscape) {
            size_t len = 0;
            while (len < ANSI_BUFFER_SIZE - 1 && m_escapeState.currentEscape[len] != '\0') {
                len++;
            }

            if (len < ANSI_BUFFER_SIZE - 1) {
                m_escapeState.currentEscape[len] = c;
                m_escapeState.currentEscape[len + 1] = '\0';
            }

            if ((c >= 'a' && c <='z') || (c >= 'A' && c <= 'Z')) {
                if (m_escapeState.currentEscape[0] == '[') {
                    int params[ANSI_MAX_PARAMS] = {0};
                    int paramCount = 0;
                    ParseANSIParams(m_escapeState.currentEscape, params, paramCount);

                    switch (c) {
                    case 'm': {
                        for (int p = 0; p < paramCount || (p == 0 && paramCount == 0); p++) {
                            int cmd = paramCount == 0 ? 0 : params[p];
                            
                            switch (cmd) {
                            case 0:
                                m_video->SetBackgroundColour(GetANSI256Colour(0));
                                m_video->SetForegroundColour(GetANSI256Colour(7));
                                break;
                            case 30: m_video->SetForegroundColour(GetANSI256Colour(0)); break;
                            case 31: m_video->SetForegroundColour(GetANSI256Colour(1)); break;
                            case 32: m_video->SetForegroundColour(GetANSI256Colour(2)); break;
                            case 33: m_video->SetForegroundColour(GetANSI256Colour(3)); break;
                            case 34: m_video->SetForegroundColour(GetANSI256Colour(4)); break;
                            case 35: m_video->SetForegroundColour(GetANSI256Colour(5)); break;
                            case 36: m_video->SetForegroundColour(GetANSI256Colour(6)); break;
                            case 37: m_video->SetForegroundColour(GetANSI256Colour(7)); break;
                            
                            case 38: // Extended Foreground
                            case 48: { // Extended Background
                                bool isForeground = (cmd == 38);
                                if (p + 2 < paramCount && params[p + 1] == 5) {
                                    if (isForeground) m_video->SetForegroundColour(GetANSI256Colour(params[p + 2]));
                                    else m_video->SetBackgroundColour(GetANSI256Colour(params[p + 2]));
                                    p += 2;
                                } else if (p + 4 < paramCount && params[p + 1] == 2) {
                                    // True Color objects are constructed dynamically
                                    Colour trueColour(params[p + 2], params[p + 3], params[p + 4]);
                                    if (isForeground) m_video->SetForegroundColour(trueColour);
                                    else m_video->SetBackgroundColour(trueColour);
                                    p += 4;
                                }
                                break;
                            }
                            
                            case 39: m_video->SetForegroundColour(GetANSI256Colour(15)); break;
                            case 40: m_video->SetBackgroundColour(GetANSI256Colour(0)); break;
                            case 41: m_video->SetBackgroundColour(GetANSI256Colour(1)); break;
                            case 42: m_video->SetBackgroundColour(GetANSI256Colour(2)); break;
                            case 43: m_video->SetBackgroundColour(GetANSI256Colour(3)); break;
                            case 44: m_video->SetBackgroundColour(GetANSI256Colour(4)); break;
                            case 45: m_video->SetBackgroundColour(GetANSI256Colour(5)); break;
                            case 46: m_video->SetBackgroundColour(GetANSI256Colour(6)); break;
                            case 47: m_video->SetBackgroundColour(GetANSI256Colour(7)); break;
                            case 49: m_video->SetBackgroundColour(GetANSI256Colour(0)); break;
                            }
                        }
                        break;
                    }
                    case 'J': { // Erase Display
                        if (paramCount > 0 && params[0] == 2) {
                            m_video->ClearScreen();
                            m_video->SetCursor(0, 0);
                        }
                        break;
                    }
                    case 'H': // Cursor position (Row;Column)
                    case 'f': {
                        // ANSI rows/cols are 1-indexed. Default to 0 if omitted.
                        uint64_t row = (paramCount > 0 && params[0] > 0) ? params[0] - 1 : 0;
                        uint64_t col = (paramCount > 1 && params[1] > 0) ? params[1] - 1 : 0;

                        // Bound to maximum rows/cols
                        uint64_t maxRow = m_video->GetNumberOfRows() > 0 ? m_video->GetNumberOfRows() - 1 : 0;
                        uint64_t maxCol = m_video->GetNumberOfColumns() > 0 ? m_video->GetNumberOfColumns() - 1 : 0;

                        row = (row > maxRow) ? maxRow : row;
                        col = (col > maxCol) ? maxCol : col;

                        m_video->SetCursor(col * CHAR_WIDTH, row * CHAR_HEIGHT);
                        break;
                    }
                    case 'A': { // Cursor Up
                        uint64_t x, y;
                        m_video->GetCursor(x, y);
                        uint64_t moveBy = ((paramCount > 0 && params[0] > 0) ? params[0] : 1) * CHAR_HEIGHT;
                        m_video->SetCursor(x, (y > moveBy) ? y - moveBy : 0);
                        break;
                    }
                    case 'B': { // Cursor Down
                        uint64_t x, y;
                        m_video->GetCursor(x, y);
                        uint64_t moveBy = ((paramCount > 0 && params[0] > 0) ? params[0] : 1) * CHAR_HEIGHT;
                        uint64_t maxY = (m_video->GetNumberOfRows() - 1) * CHAR_HEIGHT;
                        m_video->SetCursor(x, (y + moveBy < maxY) ? y + moveBy : maxY);
                        break;
                    }
                    case 'C': { // Cursor Forward (Right)
                        uint64_t x, y;
                        m_video->GetCursor(x, y);
                        uint64_t moveBy = ((paramCount > 0 && params[0] > 0) ? params[0] : 1) * CHAR_WIDTH;
                        uint64_t maxX = (m_video->GetNumberOfColumns() - 1) * CHAR_WIDTH;
                        m_video->SetCursor((x + moveBy < maxX) ? x + moveBy : maxX, y);
                        break;
                    }
                    case 'D': { // Cursor Back (Left)
                        uint64_t x, y;
                        m_video->GetCursor(x, y);
                        uint64_t moveBy = ((paramCount > 0 && params[0] > 0) ? params[0] : 1) * CHAR_WIDTH;
                        m_video->SetCursor((x > moveBy) ? x - moveBy : 0, y);
                        break;
                    }
                    }
                }
                m_escapeState.inEscape = false;
                m_escapeState.currentEscape[0] = '0';
            }
        } else {
            switch (c) {
            case '\x1b':
                m_escapeState.inEscape = true;
                m_escapeState.currentEscape[0] = '\0';
                break;
            case '\n':
            case '\v':
                m_video->NewLine();
                break;
            case '\b':
                m_video->Backspace();
                break;
            case '\r': {
                uint64_t x, y;
                m_video->GetCursor(x, y);
                m_video->SetCursor(0, y);
                break;
            }
            case '\t':
                for (int j = 0; j < 4; j++)
                    m_video->PrintChar(' ');
                break;
            case '\f':
                m_video->ClearScreen();
                m_video->SetCursor(0, 0);
                break;
            default:
                m_video->PrintChar(c);
                break;
            }
        }
    }

    if (flush && m_outputBackend != nullptr)
        m_outputBackend->Flush();
    return ESUCCESS;
}

uint64_t GraphicalTTY::GetMaxSeek() const {
    if (m_video == nullptr)
        return UINT64_MAX;
    return m_video->GetNumberOfColumns() * m_video->GetNumberOfRows();
}

uint64_t GraphicalTTY::GetCurrentSeek() const {
    if (m_video == nullptr)
        return UINT64_MAX;
    uint64_t x, y;
    m_video->GetCursor(x, y);
    return m_video->GetNumberOfColumns() * y + x;
}

void GraphicalTTY::SetVideoDevice(VideoDevice* video) {
    m_video = video;
}

VideoDevice* GraphicalTTY::GetVideoDevice() const {
    return m_video;
}
