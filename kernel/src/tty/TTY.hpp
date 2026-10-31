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

#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>

#include <DataStructures/Buffer.hpp>

#include <HAL/drivers/Device.hpp>
#include <HAL/drivers/DeviceManager.hpp>

#include <HAL/Processor.hpp>

#include <Scheduling/Event.hpp>

#include "Termios.hpp"

#define DEBUG_MIRRORING_DEFAULT_ENABLED true

#define TTY_INTERNAL_BUFFER_SIZE 512
#define TTY_READ_BUFFER_SIZE 4096

#define TTY_ENTRY_EOL 0x100
#define TTY_ENTRY_EOF 0x200

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

enum class TTYAccess {
    Read,
    Write,
    Change
};

class Process;

typedef int (*termiosCallback)(void* data, termios_t* termios);

class TTY : public Device {
public:
    TTY();
    TTY(TTYType type);
    virtual ~TTY();

    void Init();

    int Init(DeviceManager::DeviceInfo* info) override;

    int Open(int flags, const Credential& cred) override;
    int Close(int flags, const Credential& cred) override;
    int Read(void* out, size_t size, int flags, uint64_t offset, size_t* bytesRead, const Credential& cred) override;
    int Write(const void* in, size_t size, int flags, uint64_t offset, size_t* bytesWritten, const Credential& cred) override;
    int Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) override;
    int Munmap() override;
    int Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential& cred) override;
    int Isatty() override;

    virtual EventWaitQueue* GetPollQueue(uint32_t events) override;
    virtual uint32_t PollReady(uint32_t events) override;

    int Read(char* buf, size_t size, size_t* realCount = nullptr, bool nonBlock = false);
    int Write(const char* buf, size_t size, bool flush = false, size_t* realCount = nullptr);
    int WriteDebug(const char* buf, size_t size);

    void InputBytes(const char* data, size_t n);

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

    int64_t GetSession() const;
    int64_t GetForegroundPGID() const;
    bool AttachSession(Process* leader);
    bool TryAttachSession(Process* leader);
    void DetachSession();
    int CheckBackgroundAccess(Process* proc, TTYAccess kind);

    bool ApplicationCursorKeys() const;
    void SetApplicationCursorKeys(bool on);

    void ResetTerminalState();

protected:
    virtual void PutChars(const char* s, size_t n) = 0;
    virtual void FlushHW() {}
    virtual uint64_t GetColumn() { return 0; }
    virtual int OnTermiosChanged(const termios_t&) { return 0; }
    virtual int GetSize(winsize_t*) = 0;
    virtual int SetSize(const winsize_t*) = 0;
    virtual void ResetOutputState() {}

    uint8_t EchoChar(char c); // echo c per termios flags, returns columns occupied
    bool EraseBufferedChar(); // remove last char of the pending line and erase its echo

    void HandleInputLocked(char c);
    void ApplyTermios(const termios_t& t, bool flushInput);
    void FlushInput();
    void FlushInputLocked();
    void CommitLineLocked();
    void RetagPendingAsLineLocked();

    termios_t DefaultTermios();

    bool m_debugMirroring;

    termios_t m_termios;

    char m_internalBuffer[TTY_INTERNAL_BUFFER_SIZE];
    uint8_t m_internalWidths[TTY_INTERNAL_BUFFER_SIZE];
    uint64_t m_internalBufferOffset;

    // interrupt-safe SPMC ringbuffer
    RingBuffer<uint16_t, TTY_READ_BUFFER_SIZE> m_readBuffer;
    Mutex m_readLock;

    EventWaitQueue m_inputWaitQueue;

    int64_t m_session;
    int64_t m_foregroundPGID;

    spinlock_t m_inputLock;
    mutable spinlock_t m_outputLock;
    mutable int m_outputIntState;

    DeviceManager::DeviceInfo* m_info;

    bool m_appCursor = false;

private:
    TTYType m_type;
};

extern TTY* g_CurrentTTY;
extern TTY* g_KTTY;

#endif /* _TTY_HPP */