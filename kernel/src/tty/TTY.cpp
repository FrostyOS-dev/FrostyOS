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

#include <debug.h>

#include <errno.h>
#include <spinlock.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <frostyos/asm/ioctls.h>

#include <fs/VFS.hpp>

#include <HAL/drivers/DeviceManager.hpp>

#include <Scheduling/Event.hpp>
#include <Scheduling/Scheduler.hpp>

#include <SystemCalls/File.hpp>
#include <SystemCalls/Signal.hpp>
#include <SystemCalls/SystemCall.hpp>

#define TTY_WRITE_CHUNK_SIZE 64

TTY* g_CurrentTTY = nullptr;
TTY* g_KTTY = nullptr;

static inline bool IsSpecialChar(char c, char special) {
    return special != '\0' && c == special;
}

TTY::TTY() : m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_termios(), m_internalBufferOffset(0), m_session(-1), m_foregroundPGID(-1), m_inputLock(SPINLOCK_DEFAULT_VALUE), m_outputLock(SPINLOCK_DEFAULT_VALUE), m_outputIntState(-1), m_info(nullptr), m_appCursor(false), m_type(TTYType::Invalid) {
    Init();
}

TTY::TTY(TTYType type) : m_debugMirroring(DEBUG_MIRRORING_DEFAULT_ENABLED), m_termios(), m_internalBufferOffset(0), m_session(-1), m_foregroundPGID(-1), m_inputLock(SPINLOCK_DEFAULT_VALUE), m_outputLock(SPINLOCK_DEFAULT_VALUE), m_outputIntState(-1), m_info(nullptr), m_appCursor(false), m_type(type) {
    Init();
}

TTY::~TTY() {

}

void TTY::Init() {
    ApplyTermios(DefaultTermios(), true);
}

int TTY::Init(DeviceManager::DeviceInfo* info) {
    if (info->manager == nullptr)
        return -ENODEV;
    if (this != g_KTTY)
        return -EIO;

    m_info = info;

    DeviceManager::CreateParams params{};
    params.info = info;
    params.name = "console";
    params.type = FS::VType::CHR;
    return info->manager->CreateRootItem(params);
}

int TTY::Open(int flags, const Credential& cred) {
    if ((flags & O_NOCTTY) == 0) {
        Thread* t = Thread::GetCurrentThread();
        Process* proc = t != nullptr ? t->GetParent() : nullptr;
        if (proc != nullptr && proc->GetPID() == proc->GetSID() && proc->GetControllingTTY() == nullptr)
            TryAttachSession(proc);
    }
    return ESUCCESS;
}

int TTY::Close(int flags, const Credential& cred) {
    return ESUCCESS;
}

int TTY::Read(void* out, size_t size, int flags, uint64_t, size_t* bytesRead, const Credential& cred) {
    if (int rc = CheckBackgroundAccess(Thread::GetCurrentThread()->GetParent(), TTYAccess::Read); rc < 0)
        return rc;
    return Read((char*)out, size, bytesRead, (flags & O_NONBLOCK) > 0);
}

int TTY::Write(const void* in, size_t size, int flags, uint64_t, size_t* bytesWritten, const Credential& cred) {
    int rc = CheckBackgroundAccess(Thread::GetCurrentThread()->GetParent(), TTYAccess::Write);
    if (rc < 0)
        return rc;

    const char* p = static_cast<const char*>(in);
    size_t written = 0;
    while (written < size) { // chunk in TTY_WRITE_CHUNK_SIZE blocks to prevent the lock being held for large writes
        size_t chunk = MIN(static_cast<size_t>(TTY_WRITE_CHUNK_SIZE), size - written);
        size_t n = 0;
        Lock(TTYStream::OUT);
        rc = Write(p + written, chunk, true, &n);
        Unlock(TTYStream::OUT);
        written += n;
        if (rc < 0 || n == 0)
            break;
        PAUSE();
    }
    if (bytesWritten != nullptr)
        *bytesWritten = written;
    return rc < 0 && written == 0 ? rc : 0;
}

int TTY::Mmap(uint64_t offset, size_t size, VMM::MemoryObject** obj, const Credential& cred) {
    return -ENOTSUP;
}

int TTY::Munmap() {
    return -ENOTSUP;
}

int TTY::Ioctl(size_t op, void* arg, int* result, Process* proc, const Credential&) {
    switch (op) {
    case TIOCGWINSZ: {
        winsize_t size = {0, 0, 0, 0};
        int rc = GetSize(&size);
        if (rc < 0)
            return -rc;
        if (!UserWrite(arg, &size, sizeof(winsize_t), proc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case TIOCSWINSZ: {
        winsize_t size;
        if (!UserRead(arg, &size, sizeof(winsize_t), proc))
            return EFAULT;
        winsize_t before = {}, after = {};
        GetSize(&before);
        int rc = SetSize(&size);
        if (rc < 0)
            return -rc;
        GetSize(&after);
        if (memcmp(&before, &after, sizeof(winsize_t)) != 0) {
            int64_t fg = GetForegroundPGID();
            if (fg > 0)
                Scheduler::SignalProcessGroup(fg, SIGWINCH);
        }
        *result = 0;
        return 0;
    }
    case TCGETS:
        if (!UserWrite(arg, &m_termios, sizeof(termios_t), proc))
            return EFAULT;
        *result = 0;
        return 0;
    case TCSETSW: // output is synchronous, so TCSETSW is the same as TCSETS
    case TCSETSF:
    case TCSETS: {
        if (int rc = CheckBackgroundAccess(Thread::GetCurrentThread()->GetParent(), TTYAccess::Change); rc < 0)
            return -rc;
        termios_t termios;
        if (!UserRead(arg, &termios, sizeof(termios_t), proc))
            return EFAULT;
        ApplyTermios(termios, op == TCSETSF);
        int rc = OnTermiosChanged(termios);
        *result = 0;
        return rc < 0 ? -rc : rc;
    }
    case TCFLSH: {
        if (int rc = CheckBackgroundAccess(Thread::GetCurrentThread()->GetParent(), TTYAccess::Change); rc < 0)
            return -rc;
        int sel;
        if (!UserRead(arg, &sel, sizeof(int), proc))
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
        if (int rc = CheckBackgroundAccess(Thread::GetCurrentThread()->GetParent(), TTYAccess::Change); rc < 0)
            return -rc;
        *result = 0;
        return 0;
    case TIOCSCTTY: {
        if (proc->GetPID() != proc->GetSID()) 
            return EPERM;
        if (proc->GetControllingTTY() != nullptr)
            return EPERM;
        if (!TryAttachSession(proc))
            return EPERM;
        *result = 0;
        return 0;
    }
    case TIOCGPGRP: {
        if (GetSession() == -1 || proc->GetSID() != GetSession())
            return ENOTTY;
        int pg = static_cast<int>(GetForegroundPGID());
        if (!UserWrite(arg, &pg, sizeof(int), proc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case TIOCSPGRP: {
        if (GetSession() == -1 || proc->GetSID() != GetSession())
            return ENOTTY;
        int pg;
        if (!UserRead(arg, &pg, sizeof(int), proc))
            return EFAULT;
        if (pg <= 0)
            return EINVAL;
        if (!Scheduler::ProcessGroupExists(pg, proc->GetSID()))
            return EPERM;

        if (int rc = CheckBackgroundAccess(proc, TTYAccess::Change); rc < 0)
            return -rc;
        __atomic_store_n(&m_foregroundPGID, pg, __ATOMIC_RELEASE);
        *result = 0;
        return 0;
    }
    case TIOCGSID: {
        int s = static_cast<int>(GetSession());
        if (s == -1 || proc->GetSID() != s)
            return ENOTTY;
        if (!UserWrite(arg, &s, sizeof(int), proc))
            return EFAULT;
        *result = 0;
        return 0;
    }
    case FIONREAD: {
        int count = m_readBuffer.size();
        if (!UserWrite(arg, &count, sizeof(count), proc))
            return EFAULT;
        *result = 0;
        return ESUCCESS;
    }
    }

    return ENOTTY;
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
        if (!m_readBuffer.empty())
            r |= POLLIN | POLLRDNORM;
    }
    return r & events;
}

int TTY::Read(char* buf, size_t size, size_t* realCount, bool nonBlock) {
    if (realCount != nullptr)
        *realCount = 0;
    if (size == 0)
        return 0;

    if (nonBlock) {
        if (!m_readLock.TryLock())
            return -EAGAIN;
    } else
        m_readLock.Lock();

    size_t n = 0;
    int rc = 0;
    while (true) {
        // re-read termios every pass as it might change whilst sleeping
        const bool canon = (m_termios.c_lflag & ICANON) > 0;
        const size_t vmin = canon ? 1 : MIN(static_cast<size_t>(m_termios.c_cc[VMIN]), size);
        const unsigned int vtime = canon ? 0 : m_termios.c_cc[VTIME];

        bool lineDone = false;
        uint16_t e;
        while (n < size && !lineDone && m_readBuffer.pop(e)) {
            if ((e & TTY_ENTRY_EOF) > 0) {
                if (canon)
                    lineDone = true;
                continue;
            }
            buf[n++] = static_cast<char>(e & 0xFF);
            if (canon && (e & TTY_ENTRY_EOL) > 0)
                lineDone = true;
        }

        if (canon) {
            if (lineDone || n == size)
                break;
        } else {
            if (vmin == 0 && vtime == 0)
                break;
            if (n > 0 && n >= vmin)
                break;
        }

        if (nonBlock) {
            if (n == 0)
                rc = -EAGAIN;
            break;
        }

        uint64_t timeoutMS = UINT64_MAX;
        if (!canon && vtime > 0 && (vmin == 0 || n > 0))
            timeoutMS = vtime * 100;

        EventWaitNode waitNode;
        waitNode.queue = &m_inputWaitQueue;
        waitNode.requestedEvents = POLLIN;
        EventWaitNode* nodes[] = { &waitNode };

        int wr = Event::WaitOnEvents(nodes, 1, timeoutMS); // TODO: check for lost wake-ups
        if (wr == -ETIMEDOUT)
            break;

        if (wr < 0) {
            if (n == 0)
                rc = wr;
            break;
        }
    }

    m_readLock.Unlock();
    if (realCount != nullptr)
        *realCount = n;
    return rc;
}

int TTY::Write(const char* buf, size_t size, bool flush, size_t* realCount) {
    size_t count = 0;
    for (size_t i = 0; i < size; i++) {
        if (buf[i] == '\n' && (m_termios.c_oflag & ONLCR) > 0 && (m_termios.c_oflag & OPOST) > 0) {
            char c = '\r';
            PutChars(&c, 1);
        }

        PutChars(&buf[i], 1);
        count++;
    }
    if (flush)
        FlushHW();
    if (realCount != nullptr)
        *realCount = count;
    return 0;
}

int TTY::WriteDebug(const char* buf, size_t size) {
    debug_write(buf, size);
    return 0;
}

void TTY::InputBytes(const char* data, size_t n) {
    int st = Processor::DisableInterrupts();
    spinlock_acquire(&m_inputLock);
    for (size_t i = 0; i < n; i++)
        HandleInputLocked(data[i]);
    spinlock_release(&m_inputLock);
    Processor::EnableInterrupts(st);
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
            if ((m_termios.c_lflag & NOFLSH) == 0)
                FlushInputLocked();
            int64_t fg = GetForegroundPGID();
            if (fg > 0)
                Scheduler::SignalProcessGroup(fg, signal); // sender == nullptr: kernel
            return;
        }
    }

    if ((m_termios.c_lflag & ICANON) > 0) {
        if (IsSpecialChar(c, m_termios.c_cc[VERASE])) { // backspace
            EraseBufferedChar();
            return;
        } else if (IsSpecialChar(c, m_termios.c_cc[VKILL])) {
            while (EraseBufferedChar()) {} // visually erase pending line
            return;
        }
        
        bool flush = false;
        if (IsSpecialChar(c, m_termios.c_cc[VEOF])) // EOF
            flush = true;
        else {
            uint8_t width = EchoChar(c);
            m_internalBuffer[m_internalBufferOffset] = c;
            m_internalWidths[m_internalBufferOffset] = width;
            m_internalBufferOffset++;

            if (c == '\n' || IsSpecialChar(c, m_termios.c_cc[VEOL]) || IsSpecialChar(c, m_termios.c_cc[VEOL2]) || m_internalBufferOffset == TTY_INTERNAL_BUFFER_SIZE)
                flush = true;
        }

        if (flush)
            CommitLineLocked();
    } else {
        EchoChar(c);
        m_readBuffer.push(static_cast<uint8_t>(c));
        m_inputWaitQueue.Trigger(POLLIN | POLLRDNORM);
    }
}

void TTY::FlushOutput() {
    FlushHW();
}

void TTY::Lock(TTYStream stream) const {
    if (stream != TTYStream::OUT && stream != TTYStream::DEBUG)
        return;
    int st = Processor::DisableInterrupts();
    spinlock_acquire(&m_outputLock);
    m_outputIntState = st;
}

void TTY::Unlock(TTYStream stream) const {
    if (stream != TTYStream::OUT && stream != TTYStream::DEBUG)
        return;
    int st = m_outputIntState;
    spinlock_release(&m_outputLock);
    Processor::EnableInterrupts(st);
}

void TTY::ForceUnlockAll() const {
    spinlock_release(&m_outputLock);
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

bool TTY::TryAttachSession(Process* leader) {
    int64_t sid = static_cast<int64_t>(leader->GetSID());
    int64_t expected = -1;
    if (!__atomic_compare_exchange_n(&m_session, &expected, sid, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return expected == sid;
    __atomic_store_n(&m_foregroundPGID, static_cast<int64_t>(leader->GetPGID()), __ATOMIC_RELEASE);
    Scheduler::SetSessionTTY(leader->GetSID(), this);
    return true;
}

void TTY::DetachSession() {
    ResetTerminalState();
    __atomic_store_n(&m_session, -1, __ATOMIC_RELEASE);
    __atomic_store_n(&m_foregroundPGID, -1, __ATOMIC_RELEASE);
}

int TTY::CheckBackgroundAccess(Process* proc, TTYAccess kind) {
    if (proc == nullptr || GetSession() == -1 || proc->GetSID() != GetSession())
        return 0;
    if (proc->GetPGID() == GetForegroundPGID())
        return 0;
    if (kind == TTYAccess::Write && (m_termios.c_lflag & TOSTOP) == 0)
        return 0;

    int sig = (kind == TTYAccess::Read) ? SIGTTIN : SIGTTOU;
    Thread* t = Thread::GetCurrentThread();
    bool ignored = proc->GetSignalAction(sig)->address == SIG_IGN;
    bool blocked = SIGNAL_GET(&t->GetBlockedSignals(), sig) != 0;
    if (ignored || blocked)
        return (kind == TTYAccess::Read) ? -EIO : 0;
    if (Scheduler::IsProcessGroupOrphaned(proc->GetPGID(), proc->GetSID()))
        return -EIO; // orphaned: don't signal
    Scheduler::SignalProcessGroup(proc->GetPGID(), sig);
    return -EINTR;
}

bool TTY::ApplicationCursorKeys() const {
    return __atomic_load_n(&m_appCursor, __ATOMIC_ACQUIRE);
}

void TTY::SetApplicationCursorKeys(bool on) {
    __atomic_store_n(&m_appCursor, on, __ATOMIC_RELEASE);
}

void TTY::ResetTerminalState() {
    ApplyTermios(DefaultTermios(), true);
    Lock(TTYStream::OUT);
    ResetOutputState();
    Unlock(TTYStream::OUT);
    SetApplicationCursorKeys(false);
}

uint8_t TTY::EchoChar(char c) {
    if ((m_termios.c_lflag & ECHO) == 0)
        return 0;

    unsigned char u = static_cast<unsigned char>(c);

    if (c == '\n') {
        Lock(TTYStream::OUT);
        if ((m_termios.c_oflag & ONLCR) > 0 && (m_termios.c_oflag & OPOST) > 0)
            PutChars("\r\n", 2);
        else
            PutChars("\n", 1);
        FlushHW();
        Unlock(TTYStream::OUT);
        return 0;
    }

    if (c == '\t') {
        Lock(TTYStream::OUT);
        uint8_t n = 8 - (GetColumn() % 8);
        for (uint8_t i = 0; i < n; i++)
            PutChars(" ", 1);
        FlushHW();
        Unlock(TTYStream::OUT);
        return n;
    }

    if (u < 32 || u == 0x7F) {
        if (c == '\r' || c == '\b') {
            Lock(TTYStream::OUT);
            PutChars(&c, 1);
            FlushHW();
            Unlock(TTYStream::OUT);
            return 0;
        }

        if ((m_termios.c_lflag & ECHOCTL) > 0) {
            char t[2] = {'^', (u == 0x7F) ? '?' : static_cast<char>(u + 0x40)};
            Lock(TTYStream::OUT);
            PutChars(t, 2);
            FlushHW();
            Unlock(TTYStream::OUT);
            return 2;
        }

        Lock(TTYStream::OUT);
        PutChars(&c, 1); // raw control byte, no visible width
        FlushHW();
        Unlock(TTYStream::OUT);
        return 0;
    }

    Lock(TTYStream::OUT);
    PutChars(&c, 1);
    FlushHW();
    Unlock(TTYStream::OUT);
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
        Lock(TTYStream::OUT);
        for (uint8_t i = 0; i < width; i++)
            PutChars("\b \b", 3);
        FlushHW();
        Unlock(TTYStream::OUT);
    }
    return true;
}

void TTY::ApplyTermios(const termios_t& t, bool flushInput) {
    int st = Processor::DisableInterrupts();
    spinlock_acquire(&m_inputLock);
    bool wasCanon = (m_termios.c_lflag & ICANON) > 0;
    m_termios = t;
    bool isCanon = (m_termios.c_lflag & ICANON) > 0;
    if (flushInput)
        FlushInputLocked();
    else if (wasCanon && !isCanon && m_internalBufferOffset > 0)
        RetagPendingAsLineLocked();
    spinlock_release(&m_inputLock);
    Processor::EnableInterrupts(st);
    if (wasCanon != isCanon)
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
    uint16_t c;
    while (m_readBuffer.pop(c)) {}
}

void TTY::CommitLineLocked() {
    size_t len = m_internalBufferOffset;
    size_t need = len > 0 ? len : 1;
    if (need <= m_readBuffer.capacity() - m_readBuffer.size()) {
        if (len == 0)
            m_readBuffer.push(TTY_ENTRY_EOF);
        for (size_t i = 0; i < len; i++) {
            uint16_t e = static_cast<uint8_t>(m_internalBuffer[i]);
            if (i + 1 == len)
                e |= TTY_ENTRY_EOL;
            m_readBuffer.push(e);
        }
    }
    memset(m_internalBuffer, 0, len);
    memset(m_internalWidths, 0, len);
    m_internalBufferOffset = 0;
    m_inputWaitQueue.Trigger(POLLIN | POLLRDNORM);
}

void TTY::RetagPendingAsLineLocked() {
    size_t n = m_readBuffer.size();
    for (size_t i = 0; i < n; i++) {
        uint16_t e;
        if (!m_readBuffer.pop(e))
            break;
        if (i + 1 == n)
            e |= TTY_ENTRY_EOL;
        m_readBuffer.push(e);
    }
}

termios_t TTY::DefaultTermios() {
    termios_t termios;
    memset(&termios, 0, sizeof(termios_t));
    termios.c_iflag = ICRNL;
    termios.c_oflag = ONLCR | OPOST;
    termios.c_lflag = ECHO | ICANON | ISIG | ECHOCTL;
    termios.c_cflag = CS8 | B38400;
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VINTR] = 0x3;
    termios.c_cc[VQUIT] = 0x1c;
    termios.c_cc[VERASE] = 0x7f;
    termios.c_cc[VKILL] = 0x15;
    termios.c_cc[VEOF] = 0x4;
    termios.c_cc[VSTART] = 0x11;
    termios.c_cc[VSTOP] = 0x13;
    termios.c_cc[VSUSP] = 0x1a;
    return termios;
}
