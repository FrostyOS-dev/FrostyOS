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

#include <DataStructures/Buffer.hpp>

#include <HAL/drivers/Video/VideoDevice.hpp>

#include <Scheduling/Event.hpp>

#include "FBConsole.hpp"
#include "Termios.hpp"
#include "TTYBackend.hpp"

#define DEBUG_MIRRORING_DEFAULT_ENABLED true

#define ANSI_MAX_PARAMS 16

#define TTY_INTERNAL_BUFFER_SIZE 512
#define TTY_READ_BUFFER_SIZE 4096

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

class Process;

typedef int (*termiosCallback)(void* data, termios_t* termios);

class TTY {
public:
    TTY();
    TTY(TTYType type);
    virtual ~TTY();

    virtual void Init();

    int Read(char* buf, size_t size, size_t* realCount = 0);
    int Write(const char* buf, size_t size, bool flush = false);
    int WriteDebug(const char* buf, size_t size);

    void HandleInput(char c); // Interrupt safe, only 1 user

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

    // reads/writes from/to arg are assumed to be from/to a usermode region
    int Ioctl(uint64_t op, void* arg, int* result, Process* currentProc);

    virtual int SetSize(const winsize_t* size);
    virtual int GetSize(winsize_t* size);

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

    void SetTermiosCallback(termiosCallback callback, void* data);

    static bool CanRead(TTYStream stream);
    static bool CanWrite(TTYStream stream);

protected:
    virtual int InternalWrite(const char* buf, size_t size, bool flush = false);

    uint8_t EchoChar(char c); // echo c per termios flags, returns columns occupied
    bool EraseBufferedChar(); // remove last char of the pending line and erase its echo
    uint64_t GetOutputColumn();

    TTYBackend* m_inputBackend;
    TTYBackend* m_outputBackend;
    TTYBackend* m_debugBackend;
    termios_t m_termios;
    termiosCallback m_termiosCallback;
    void* m_termiosCallbackData;
    bool m_debugMirroring;

    char m_internalBuffer[TTY_INTERNAL_BUFFER_SIZE];
    uint8_t m_internalWidths[TTY_INTERNAL_BUFFER_SIZE];
    uint64_t m_internalBufferOffset;

    // interrupt-safe SPMC ringbuffer
    RingBuffer<char, TTY_READ_BUFFER_SIZE> m_readBuffer;

    EventWaitQueue m_inputWaitQueue;

private:
    TTYType m_type;
};

class GraphicalTTY : public TTY {
public:
    GraphicalTTY();
    GraphicalTTY(FBConsole* console);
    ~GraphicalTTY() override;

    uint64_t GetMaxSeek() const override;
    uint64_t GetCurrentSeek() const override;
    
    int SetSize(const winsize_t* size) override;
    int GetSize(winsize_t* size) override;
    
    void SetConsole(FBConsole* console);
    FBConsole* GetConsole();

protected:
    int InternalWrite(const char* buf, size_t size, bool flush = false) override;

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

    uint64_t m_savedX;
    uint64_t m_savedY;
    FBConsole* m_console;
};

extern TTY* g_CurrentTTY;
extern TTY* g_KTTY;

#endif /* _TTY_HPP */