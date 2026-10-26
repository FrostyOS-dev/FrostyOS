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

#include "Termios.hpp"
#include "TTY.hpp"
#include "TTYBackend.hpp"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <frostyos/asm/ioctls.h>

#include <Graphics/VGAFont.hpp>

#include <HAL/drivers/DeviceManager.hpp>

#include <Scheduling/Event.hpp>
#include <Scheduling/Scheduler.hpp>

#include <SystemCalls/Signal.hpp>
#include <SystemCalls/SystemCall.hpp>

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
TTY* g_KTTY = nullptr;

static inline bool IsSpecialChar(char c, char special) {
    return special != '\0' && c == special;
}

TTY::TTY() : m_inputBackend(nullptr), m_outputBackend(nullptr), m_debugBackend(nullptr), m_termios(), m_termiosCallback(nullptr), m_termiosCallbackData(nullptr), m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_internalBufferOffset(0), m_eofPending(0), m_session(-1), m_foregroundPGID(-1), m_info(nullptr), m_type(TTYType::Invalid) {
    
}

TTY::TTY(TTYType type) : m_inputBackend(nullptr), m_outputBackend(nullptr), m_debugBackend(nullptr), m_termios(), m_termiosCallback(nullptr), m_termiosCallbackData(nullptr), m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_internalBufferOffset(0), m_eofPending(0), m_session(-1), m_foregroundPGID(-1), m_info(nullptr), m_type(type) {

}

TTY::~TTY() {

}

void TTY::Init() {
    m_termios.c_iflag = ICRNL;
    m_termios.c_oflag = ONLCR;
    m_termios.c_lflag = ECHO | ICANON | ISIG | ECHOCTL;
    m_termios.c_cflag = CS8 | B38400;
    m_termios.c_cc[VMIN] = 1;
    m_termios.c_cc[VINTR] = 0x3;
    m_termios.c_cc[VQUIT] = 0x1c;
    m_termios.c_cc[VERASE] = '\b';
    m_termios.c_cc[VKILL] = 0x15;
    m_termios.c_cc[VEOF] = 0x4;
    m_termios.c_cc[VSTART] = 0x11;
    m_termios.c_cc[VSTOP] = 0x13;
    m_termios.c_cc[VSUSP] = 0x1a;
}

int TTY::Init(DeviceManager::DeviceInfo* info) {
    if (info->manager == nullptr)
        return -ENODEV;
    if (this != g_KTTY)
        return -EIO;

    m_info = info;

    return info->manager->CreateRootFile(info, "console");
}

int TTY::Open(int flags, const Credential& cred) {
    return ESUCCESS;
}

int TTY::Close(int flags, const Credential& cred) {
    return ESUCCESS;
}

int TTY::Read(void* out, size_t size, int, uint64_t, size_t* bytesRead, const Credential& cred) {
    return Read((char*)out, size, bytesRead);
}

int TTY::Write(const void* in, size_t size, int, uint64_t, size_t* bytesWritten, const Credential& cred) {
    int rc = Write((const char*)in, size, true, bytesWritten);
    *bytesWritten = rc < 0 ? 0 : size;
    return rc;
}

int TTY::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
    return -ENOTSUP;
}

int TTY::Munmap() {
    return -ENOTSUP;
}

int TTY::Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential&) {
    return Ioctl(op, arg, result, proc);
}

int TTY::Isatty() {
    return 0;
}

EventWaitQueue* TTY::GetPollQueue(uint32_t events) {
    if ((events & (POLLIN | POLLRDNORM)) > 0)
        return &m_inputWaitQueue;
    return nullptr;
}

uint32_t TTY::PollReady(uint32_t events) {
    uint32_t r = POLLOUT | POLLWRNORM;
    if ((events & (POLLIN | POLLRDNORM)) > 0) {
        if (!m_readBuffer.empty() || m_eofPending)
            r |= POLLIN | POLLRDNORM;
    }
    return r & events;
}

int TTY::Read(char* buf, size_t size, size_t* realCount) {
    if (m_inputBackend == nullptr)
        return -ENODEV;
    
    uint64_t min = static_cast<int>((m_termios.c_lflag & ICANON) > 0 ? 1 : m_termios.c_cc[VMIN]); // minimum characters to read, default to 1 in non-canonical mode
    int time = (m_termios.c_lflag & ICANON) > 0 ? 0 : m_termios.c_cc[VTIME]; // maximum time to wait, default to 0 in non-canonical mode

    if (min == 0 && time == 0) { // no minimum, and no waiting, just read what is available
        uint64_t i;
        for (i = 0; i < size; i++) {
            if (!m_readBuffer.pop(buf[i]))
                break;
        }
        if (realCount != nullptr)
            *realCount = i;
        return 0;
    }

    // time is in 1/10 of a second, we want milliseconds, use UINT64_MAX to sleep indefinitely
    size_t timeMS = (time == 0) ? UINT64_MAX : (time * 100);
    size_t readCount = 0;
    bool canonical = (m_termios.c_lflag & ICANON) > 0;

    while (readCount < min && readCount < size) {
        while (readCount < size && m_readBuffer.pop(buf[readCount]))
            readCount++;

        if (readCount >= min || readCount >= size)
            break;

        if (canonical && readCount == 0 && TakePendingEOF())
            break;

        EventWaitNode waitNode;
        waitNode.queue = &m_inputWaitQueue;
        waitNode.requestedEvents = POLLIN;

        EventWaitNode* nodes[] = { &waitNode };

        // Wait for the event
        int rc = Event::WaitOnEvents(nodes, 1, timeMS);
        if (rc < 0 && rc != -ETIMEDOUT) {
            if (readCount > 0)
                break;
            return rc;
        }

        if (rc == -ETIMEDOUT)
            break;
    }

    if (realCount != nullptr)
        *realCount = readCount;
    return 0;
}

int TTY::Write(const char* buf, size_t size, bool flush, size_t* realCount) {
    int rc = 0;
    size_t count = 0;
    for (size_t i = 0; i < size; i++) {
        if (buf[i] == '\n' && (m_termios.c_oflag & ONLCR) > 0) {
            char c = '\r';
            rc = InternalWrite(&c, 1, false);
            if (rc < 0)
                break;
        }

        rc = InternalWrite(&buf[i], 1, false);
        if (rc < 0)
            break;
        count++;
    }
    if (flush && m_outputBackend != nullptr)
        m_outputBackend->Flush();
    if (realCount != nullptr)
        *realCount = count;
    return rc;
}

int TTY::WriteDebug(const char* buf, size_t size) {
    if (m_debugBackend == nullptr)
        return -ENODEV;
    for (size_t i = 0; i < size; i++)
        m_debugBackend->WriteChar(buf[i]);
    return 0;
}

void TTY::HandleInput(char c) {
    spinlock_acquire(&m_inputLock);
    HandleInputLocked(c);
    spinlock_release(&m_inputLock);
}

void TTY::HandleInputLocked(char c) {
    if (c == '\r' && (m_termios.c_iflag & IGNCR) > 0)
        return;

    if (c == '\r' && (m_termios.c_iflag & ICRNL) > 0)
        c = '\n';
    else if (c == '\n' && (m_termios.c_iflag & INLCR) > 0)
        c = '\r';

    if ((m_termios.c_lflag & ISIG) > 0) {
        int signal = -1;
        if (IsSpecialChar(c, m_termios.c_cc[VINTR]))
            signal = SIGINT;
        else if (IsSpecialChar(c, m_termios.c_cc[VQUIT]))
            signal = SIGQUIT;
        else if (IsSpecialChar(c, m_termios.c_cc[VSUSP]))
            signal = SIGTSTP;

        if (signal >= 0) {
            EchoChar(c);
            if ((m_termios.c_lflag & NOFLSH) == 0) {
                memset(m_internalBuffer, 0, m_internalBufferOffset);
                memset(m_internalWidths, 0, m_internalBufferOffset);
                m_internalBufferOffset = 0;
            }
            int64_t fg = GetForegroundPGID();
            if (fg > 0)
                Scheduler::SignalProcessGroup(fg, signal); // sender == nullptr: kernel
            return;
        }
    }

    if ((m_termios.c_lflag & ICANON) > 0) {
        if (IsSpecialChar(c, m_termios.c_cc[VERASE]) || c == 0x7F) { // backspace
            EraseBufferedChar();
            return;
        } else if (IsSpecialChar(c, m_termios.c_cc[VKILL])) {
            while (EraseBufferedChar()) {} // visually erase pending line
            return;
        }
        
        bool flush = false;
        bool eofOnEmpty = false;
        if (IsSpecialChar(c, m_termios.c_cc[VEOF])) { // EOF
            flush = true;
            eofOnEmpty = (m_internalBufferOffset == 0);
        } else {
            uint8_t width = EchoChar(c);
            m_internalBuffer[m_internalBufferOffset] = c;
            m_internalWidths[m_internalBufferOffset] = width;
            m_internalBufferOffset++;

            if (c == '\n' || IsSpecialChar(c, m_termios.c_cc[VEOL]) || IsSpecialChar(c, m_termios.c_cc[VEOL2]) || m_internalBufferOffset == TTY_INTERNAL_BUFFER_SIZE)
                flush = true;
        }

        if (flush) {
            for (size_t i = 0; i < m_internalBufferOffset; i++)
                m_readBuffer.push(m_internalBuffer[i]);

            memset(m_internalBuffer, 0, m_internalBufferOffset);
            memset(m_internalWidths, 0, m_internalBufferOffset);
            m_internalBufferOffset = 0;

            if (eofOnEmpty)
                __atomic_fetch_add(&m_eofPending, 1, __ATOMIC_RELEASE);

            m_inputWaitQueue.Trigger(POLLIN | POLLRDNORM);
        }
    } else {
        EchoChar(c);
        m_readBuffer.push(c);
        m_inputWaitQueue.Trigger(POLLIN | POLLRDNORM);
    }
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
    if (m_inputBackend != nullptr)
        m_inputBackend->Seek(pos);
    if (m_outputBackend != nullptr)
        m_outputBackend->Seek(pos);
    if (m_debugBackend != nullptr)
        m_debugBackend->Seek(pos);
}

uint64_t TTY::GetMaxSeek() const {
    return UINT64_MAX; // unknown
}

uint64_t TTY::GetCurrentSeek() const {
    return UINT64_MAX; // unknown
}

int TTY::Ioctl(uint64_t op, void* arg, int* result, Process* currentProc) {
    switch (op) {
    case TIOCGWINSZ: {
        winsize_t size = {0, 0, 0, 0};
        int rc = GetSize(&size);
        if (rc < 0)
            return -rc;
        if (!UserWrite(arg, &size, sizeof(winsize_t), currentProc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case TIOCSWINSZ: {
        winsize_t size;
        if (!UserRead(arg, &size, sizeof(winsize_t), currentProc))
            return EFAULT;
        int rc = SetSize(&size);
        if (rc < 0)
            return -rc;
        *result = 0;
        return 0;
    }
    case TCGETS:
        if (!UserWrite(arg, &m_termios, sizeof(termios_t), currentProc))
            return EFAULT;
        *result = 0;
        return 0;
    case TCSETSW: // output is synchronous, so TCSETSW is the same as TCSETS
    case TCSETSF:
    case TCSETS: {
        termios_t termios;
        if (!UserRead(arg, &termios, sizeof(termios_t), currentProc))
            return EFAULT;
        ApplyTermios(termios, op == TCSETSF);
        int rc = 0;
        if (m_termiosCallback != nullptr)
            rc = m_termiosCallback(m_termiosCallbackData, &m_termios);
        *result = 0;
        return rc;
    }
    case TCFLSH: {
        int sel;
        if (!UserRead(arg, &sel, sizeof(int), currentProc))
            return EFAULT;
        if (sel < 0 || sel > 2) {
            return EINVAL;
        }
        if (sel != 1)
            FlushInput();
        *result = 0;
        return 0;
    }
    case TCSBRK:
        *result = 0;
        return 0;
    case TIOCSCTTY: {
        if (currentProc->GetPID() != currentProc->GetSID()) 
            return EPERM;
        if (currentProc->GetControllingTTY() != nullptr)
            return EPERM;
        int64_t cur = GetSession();
        if (cur != -1 && cur != currentProc->GetSID())
            return EPERM;
        AttachSession(currentProc);
        *result = 0;
        return 0;
    }
    case TIOCGPGRP: {
        if (GetSession() == -1 || currentProc->GetSID() != GetSession())
            return ENOTTY;
        int pg = static_cast<int>(GetForegroundPGID());
        if (!UserWrite(arg, &pg, sizeof(int), currentProc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case TIOCSPGRP: {
        if (GetSession() == -1 || currentProc->GetSID() != GetSession())
            return ENOTTY;
        int pg;
        if (!UserRead(arg, &pg, sizeof(int), currentProc))
            return EFAULT;
        if (pg <= 0)
            return EINVAL;
        if (!Scheduler::ProcessGroupExists(pg, currentProc->GetSID()))
            return EPERM;

        if (currentProc->GetPGID() != GetForegroundPGID()) {
            Thread* t = Thread::GetCurrentThread();
            bool ignored = currentProc->GetSignalAction(SIGTTOU)->address == SIG_IGN;
            bool blocked = SIGNAL_GET(&t->GetBlockedSignals(), SIGTTOU) != 0;
            if (!ignored && !blocked) {
                if (Scheduler::IsProcessGroupOrphaned(currentProc->GetPGID(), currentProc->GetSID()))
                    return EIO;
                Scheduler::SignalProcessGroup(currentProc->GetPGID(), SIGTTOU);
                return EINTR;
            }
        }
        __atomic_store_n(&m_foregroundPGID, pg, __ATOMIC_RELEASE);
        *result = 0;
        return 0;
    }
    case TIOCGSID: {
        int s = static_cast<int>(GetSession());
        if (s == -1 || currentProc->GetSID() != s)
            return ENOTTY;
        if (!UserWrite(arg, &s, sizeof(int), currentProc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case FIONREAD:
        return ENOSYS;
    }

    return EINVAL;
}

int TTY::SetSize(const winsize_t* size) {
    return -ENOSYS;
}

int TTY::GetSize(winsize_t* size) {
    return -ENOSYS;
}

void TTY::FlushOutput() {
    if (m_outputBackend != nullptr)
        m_outputBackend->Flush();
}

void TTY::Lock(TTYStream stream) const {
    switch (stream) {
    case TTYStream::IN:
        if (m_inputBackend != nullptr)
            m_inputBackend->Lock();
        break;
    case TTYStream::OUT:
        if (m_outputBackend != nullptr)
            m_outputBackend->Lock();
        break;
    case TTYStream::DEBUG:
        if (m_debugBackend != nullptr)
            m_debugBackend->Lock();
        break;
    default:
        break;
    }
}

void TTY::Unlock(TTYStream stream) const {
    switch (stream) {
    case TTYStream::IN:
        if (m_inputBackend != nullptr)
            m_inputBackend->Unlock();
        break;
    case TTYStream::OUT:
        if (m_outputBackend != nullptr)
            m_outputBackend->Unlock();
        break;
    case TTYStream::DEBUG:
        if (m_debugBackend != nullptr)
            m_debugBackend->Unlock();
        break;
    default:
        break;
    }
}

void TTY::ForceUnlockAll() const {
    if (m_inputBackend != nullptr)
        m_inputBackend->ForceUnlock();
    if (m_outputBackend != nullptr)
        m_outputBackend->ForceUnlock();
    if (m_debugBackend != nullptr)
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

void TTY::SetTermiosCallback(termiosCallback callback, void* data) {
    m_termiosCallback = callback;
    m_termiosCallbackData = data;
}

EventWaitQueue* TTY::GetPollQueue(TTYStream s) {
    return s == TTYStream::IN ? &m_inputWaitQueue : nullptr;
}

uint32_t TTY::PollReady(TTYStream s, uint32_t events) {
    uint32_t r = 0;
    if (s == TTYStream::IN) {
        if (!m_readBuffer.empty() || m_eofPending)
            r |= POLLIN | POLLRDNORM;
    } else
        r |= POLLOUT | POLLWRNORM;
    return r & events;
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

int64_t TTY::GetSession() const {
    return __atomic_load_n(&m_session, __ATOMIC_ACQUIRE);
}

int64_t TTY::GetForegroundPGID() const {
    return __atomic_load_n(&m_foregroundPGID, __ATOMIC_ACQUIRE);
}

bool TTY::AttachSession(Process* leader) {
    __atomic_store_n(&m_foregroundPGID, leader->GetPGID(), __ATOMIC_RELEASE);
    __atomic_store_n(&m_session, leader->GetSID(), __ATOMIC_RELEASE);
    Scheduler::SetSessionTTY(leader->GetSID(), this);
    return true;
}

void TTY::DetachSession() {
    __atomic_store_n(&m_session, -1, __ATOMIC_RELEASE);
    __atomic_store_n(&m_foregroundPGID, -1, __ATOMIC_RELEASE);
}

int TTY::CheckBackgroundAccess(Process* proc, bool write) {
    if (proc == nullptr || GetSession() == -1 || proc->GetSID() != GetSession())
        return 0;
    if (proc->GetPGID() == GetForegroundPGID())
        return 0;
    if (write && (m_termios.c_lflag & TOSTOP) == 0)
        return 0;

    int sig = write ? SIGTTOU : SIGTTIN;
    Thread* t = Thread::GetCurrentThread();
    bool ignored = proc->GetSignalAction(sig)->address == SIG_IGN;
    bool blocked = SIGNAL_GET(&t->GetBlockedSignals(), sig) != 0;
    if (ignored || blocked)
        return write ? 0 : -EIO;
    if (Scheduler::IsProcessGroupOrphaned(proc->GetPGID(), proc->GetSID()))
        return -EIO; // orphaned: don't signal
    Scheduler::SignalProcessGroup(proc->GetPGID(), sig);
    return -EINTR;
}

int TTY::InternalWrite(const char* buf, size_t size, bool flush) {
    if (m_outputBackend == nullptr)
        return -ENODEV;
    for (size_t i = 0; i < size; i++) {
        m_outputBackend->WriteChar(buf[i]);
        if (m_debugMirroring && m_debugBackend != nullptr)
            m_debugBackend->WriteChar(buf[i]);
        if (m_inputBackend != nullptr && m_inputBackend != m_outputBackend)
            m_inputBackend->WriteChar(buf[i]);
    }
    if (flush)
        m_outputBackend->Flush();
    return 0;
}

uint8_t TTY::EchoChar(char c) {
    if ((m_termios.c_lflag & ECHO) == 0)
        return 0;

    unsigned char u = static_cast<unsigned char>(c);

    if (c == '\n') {
        if ((m_termios.c_oflag & ONLCR) > 0)
            InternalWrite("\r\n", 2, true);
        else
            InternalWrite("\n", 1, true);
        return 0;
    }

    if (c == '\t') {
        uint8_t n = 8 - (GetOutputColumn() % 8);
        for (uint8_t i = 0; i < n; i++)
            InternalWrite(" ", 1, i + 1 == n);
        return n;
    }

    if (u < 32 || u == 0x7F) {
        if (c == '\r' || c == '\b') {
            InternalWrite(&c, 1, true);
            return 0;
        }

        if ((m_termios.c_lflag & ECHOCTL) > 0) {
            char t[2] = {'^', (u == 0x7F) ? '?' : static_cast<char>(u + 0x40)};
            InternalWrite(t, 2, true);
            return 2;
        }

        InternalWrite(&c, 1, true); // raw control byte, no visible width
        return 0;
    }

    InternalWrite(&c, 1, true);
    return 1;
}

bool TTY::EraseBufferedChar() {
    if (m_internalBufferOffset == 0)
        return false;

    m_internalBufferOffset--;
    uint8_t width = m_internalWidths[m_internalBufferOffset];
    m_internalBuffer[m_internalBufferOffset] = '\0';
    m_internalWidths[m_internalBufferOffset] = 0;

    if ((m_termios.c_lflag & ECHO) > 0) {
        for (uint8_t i = 0; i < width; i++)
            InternalWrite("\b \b", 3, i + 1 == width);
    }
    return true;
}

uint64_t TTY::GetOutputColumn() {
    uint64_t x = 0, y = 0;
    if (m_outputBackend != nullptr)
        m_outputBackend->GetCursor(x, y);
    return x;
}

bool TTY::TakePendingEOF() {
    uint32_t n = __atomic_load_n(&m_eofPending, __ATOMIC_ACQUIRE);
    while (n > 0) {
        if (__atomic_compare_exchange_n(&m_eofPending, &n, n - 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return true;
    }
    return false;
}

void TTY::ApplyTermios(const termios_t& t, bool flushInput) {
    bool commit = false;
    int st = Processor::DisableInterrupts();
    spinlock_acquire(&m_inputLock);
    bool wasCanon = (m_termios.c_lflag & ICANON) > 0;
    m_termios = t;
    bool isCanon = (m_termios.c_lflag & ICANON) > 0;
    if (flushInput)
        FlushInputLocked();
    else if (wasCanon && !isCanon && m_internalBufferOffset > 0) {
        for (uint64_t i = 0; i< m_internalBufferOffset; i++)
            m_readBuffer.push(m_internalBuffer[i]);
        memset(m_internalBuffer, 0, m_internalBufferOffset);
        memset(m_internalWidths, 0, m_internalBufferOffset);
        m_internalBufferOffset = 0;
        commit = true;
    }
    spinlock_release(&m_inputLock);
    Processor::EnableInterrupts(st);
    if (commit)
        m_inputWaitQueue.Trigger(POLLIN | POLLRDNORM);
}

void TTY::FlushInput() {
    int st = Processor::DisableInterrupts();
    spinlock_acquire(&m_inputLock);
    FlushInputLocked();
    spinlock_release(&m_inputLock);
    Processor::EnableInterrupts(st);
}

void TTY::FlushInputLocked() {
    memset(m_internalBuffer, 0, m_internalBufferOffset);
    memset(m_internalWidths, 0, m_internalBufferOffset);
    m_internalBufferOffset = 0;
    char c;
    while (m_readBuffer.pop(c)) {}
    __atomic_store_n(&m_eofPending, 0, __ATOMIC_RELEASE);
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

int GraphicalTTY::InternalWrite(const char* buf, size_t size, bool flush) {
    if (m_console == nullptr)
        return -ENODEV;

    for (uint64_t i = 0; i < size; i++) {
        char c = buf[i];

        if (m_debugMirroring && m_debugBackend != nullptr)
            m_debugBackend->WriteChar(c);

        if (m_inputBackend != nullptr && m_inputBackend != m_outputBackend)
            m_inputBackend->WriteChar(c);

        ProcessChar(c);
    }

    if (flush && m_outputBackend != nullptr)
        m_outputBackend->Flush();
    return ESUCCESS;
}

uint64_t GraphicalTTY::GetMaxSeek() const {
    if (m_console == nullptr)
        return UINT64_MAX;
    return m_console->GetNumberOfColumns() * m_console->GetNumberOfRows();
}

uint64_t GraphicalTTY::GetCurrentSeek() const {
    if (m_console == nullptr)
        return UINT64_MAX;
    uint64_t x, y;
    m_console->GetCursor(x, y);
    return (y / CHAR_HEIGHT) * m_console->GetNumberOfColumns() + (x / CHAR_WIDTH);
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

int GraphicalTTY::GetSize(winsize_t* size) {
    if (m_console == nullptr)
        return -ENODEV;
    size->ws_col = m_console->GetNumberOfColumns();
    size->ws_row = m_console->GetNumberOfRows();
    size->ws_xpixel = m_console->GetWidth();
    size->ws_ypixel = m_console->GetHeight();
    return ESUCCESS;
}

void GraphicalTTY::SetConsole(FBConsole* console) {
    m_console = console;
}

FBConsole* GraphicalTTY::GetConsole() {
    return m_console;
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
            ResetAttributes();
            ApplyAttributes();
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
        } else if (c == ':' || (c >= 0x20 && c <= 0x2F))
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
    if (m_escapeState.privateMarker != 0)
        return; // unsupported, just ignore

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
