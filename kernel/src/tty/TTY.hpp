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

#include <stddef.h>
#include <stdint.h>

#include <HAL/drivers/Video/VideoDevice.hpp>

#include "TTYBackend.hpp"

#define DEBUG_MIRRORING_DEFAULT_ENABLED true

#define ANSI_BUFFER_SIZE 64

enum class TTYStream {
    IN,
    OUT,
    DEBUG,
    INVALID
};

enum class TTYType {
    Graphical,
    Serial,
    Invalid
};

class TTY {
public:
    TTY();
    TTY(TTYType type);
    virtual ~TTY();

    virtual void Init();

    int Read(char* buf, size_t size, size_t* realCount = 0); // Only blocks for the first character, and reads any waiting to be read
    int ReadBlock(char* buf, size_t size);
    virtual int Write(const char* buf, size_t size, bool flush = false);
    int WriteDebug(const char* buf, size_t size);

    void SetCursor(uint64_t x, uint64_t y);
    void GetCursor(uint64_t& x, uint64_t& y);

    void SetInputBackend(TTYBackend* backend);
    void SetOutputBackend(TTYBackend* backend);
    void SetDebugBackend(TTYBackend* backend);

    TTYBackend* GetInputBackend() const;
    TTYBackend* GetOutputBackend() const;
    TTYBackend* GetDebugBackend() const;

    void Seek(uint64_t pos);
    virtual uint64_t GetMaxSeek() const; // returns UINT64_MAX if unknown
    virtual uint64_t GetCurrentSeek() const; // returns UINT64_MAX if unknown

    void FlushOutput();

    void Lock(TTYStream stream) const;
    void Unlock(TTYStream stream) const;
    void ForceUnlockAll() const;

    // Debug mirroring is for mirroring any writes to the debug backend. Reads are not mirrored.
    void EnableDebugMirroring();
    void DisableDebugMirroring();
    bool IsDebugMirroring() const;

    TTYType GetType() const;
    void SetType(TTYType type);

    static bool CanRead(TTYStream stream);
    static bool CanWrite(TTYStream stream);

protected:
    TTYBackend* m_inputBackend;
    TTYBackend* m_outputBackend;
    TTYBackend* m_debugBackend;
    bool m_debugMirroring;

private:
    TTYType m_type;
};

class GraphicalTTY : public TTY {
public:
    GraphicalTTY();
    GraphicalTTY(VideoDevice* video);
    ~GraphicalTTY() override;

    void Init() override;

    int Write(const char* buf, size_t size, bool flush = false) override;

    uint64_t GetMaxSeek() const override;
    uint64_t GetCurrentSeek() const override;

    void SetVideoDevice(VideoDevice* video);
    VideoDevice* GetVideoDevice() const;

private:
    struct EscapeState {
        bool inEscape;
        char currentEscape[ANSI_BUFFER_SIZE];
    } m_escapeState;
    VideoDevice* m_video;
};

extern TTY* g_CurrentTTY;

#endif /* _TTY_HPP */