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

#include "Socket.hpp"
#include "UnixSocket.hpp"

#include <errno.h>
#include <string.h>
#include <util.h>

#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <frostyos/asm/ioctls.h>

#include <Scheduling/Event.hpp>

#include <SystemCalls/Socket.hpp>
#include <SystemCalls/SystemCall.hpp>

namespace Net {
    static constexpr size_t DIRECTION_LIMIT = 65536;
    static constexpr size_t STREAM_CHUNK = 4096;
    static constexpr size_t STREAM_MAX_ENTRY = 16384;
    static constexpr size_t MAX_BACKLOG = 128;
    static constexpr uint32_t ALL_EVENTS = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP | POLLRDNORM | POLLRDBAND | POLLWRNORM | POLLRDHUP;
    
    static int WaitOnQueue(EventWaitQueue& queue, Mutex& heldLock) {
        EventWaitNode node{};
        node.queue = &queue;
        node.requestedEvents = ALL_EVENTS;
        EventWaitNode* nodes[] = {&node};
        Event::RegisterWaitNodes(nodes, 1);
        heldLock.Unlock();
        int rc = Event::BlockOnWaitNodes(nodes, 1, UINT64_MAX);
        Event::UnregisterWaitNodes(nodes, 1);
        heldLock.Lock();
        return rc < 0 ? -rc : 0;
    }

    struct UnixMessage {
        UnixMessage* next;
        size_t capacity;
        size_t length;
        size_t readOffset;
        uint32_t fdSlots;
        uint32_t fdCount;
        bool sealed;

        FileDescriptor** Fds() {
            return reinterpret_cast<FileDescriptor**>(this + 1);
        }
        uint8_t* Data() {
            return reinterpret_cast<uint8_t*>(Fds() + fdSlots);
        }
        size_t Charge() const {
            return sizeof(UnixMessage) + fdSlots * sizeof(FileDescriptor*) + capacity;
        }
        size_t Remaining() const {
            return length - readOffset;
        }
    };

    static size_t MessageCharge(size_t fdSlots, size_t capacity) {
        return sizeof(UnixMessage) + fdSlots * sizeof(FileDescriptor*) + capacity;
    }

    static UnixMessage* AllocMessage(size_t fdSlots, size_t capacity) {
        void* mem = kcalloc(1, MessageCharge(fdSlots, capacity));
        if (mem == nullptr)
            return nullptr;
        UnixMessage* m = static_cast<UnixMessage*>(mem);
        m->capacity = capacity;
        m->fdSlots = (uint32_t)fdSlots;
        return m;
    }

    static void FreeMessageList(UnixMessage* m) {
        while (m != nullptr) {
            UnixMessage* next = m->next;
            for (uint32_t i = 0; i < m->fdCount; i++)
                m->Fds()[i]->Unref();
            kfree(m);
            m = next;
        }
    }

    struct UnixDirection {
        Mutex lock;
        UnixMessage* head = nullptr;
        UnixMessage* tail = nullptr;
        size_t charged = 0;
        size_t readable = 0;
        size_t messages = 0;
        bool writerDone = false;
        bool readerDone = false;

        UnixMessage* DetachAll() {
            UnixMessage* list = head;
            head = nullptr;
            tail = nullptr;
            charged = 0;
            readable = 0;
            messages = 0;
            return list;
        }
    };

    struct UnixConnection {
        uint64_t refCount = 1;
        UnixDirection dir[2];
        EventWaitQueue waitq[2];
        PeerCred cred[2];
        UnixConnection* pendingNext = nullptr;

        void Ref() {
            __atomic_add_fetch(&refCount, 1, __ATOMIC_ACQ_REL);
        }
        void Unref() {
            if (__atomic_sub_fetch(&refCount, 1, __ATOMIC_ACQ_REL) == 0) {
                FreeMessageList(dir[0].DetachAll());
                FreeMessageList(dir[1].DetachAll());
                delete this;
            }
        }
    };

    static void ShutdownSide(UnixConnection* c, int side , bool shutRead, bool shutWrite) {
        if (shutWrite) {
            UnixDirection& out = c->dir[side];
            out.lock.Lock();
            out.writerDone = true;
            out.lock.Unlock();
        }
        if (shutRead) {
            UnixDirection& in = c->dir[1 - side];
            in.lock.Lock();
            in.readerDone = true;
            UnixMessage* dead = in.DetachAll();
            in.lock.Unlock();
            FreeMessageList(dead);
        }
        c->waitq[side].Trigger(ALL_EVENTS);
        c->waitq[1 - side].Trigger(ALL_EVENTS);
    }

    static int ParseSockAddr(const void* addr, size_t addrLen, char* pathOut, size_t* pathLenOut) {
        const size_t headerLen = offsetof(SockAddrUn, path);
        if (addr == nullptr || addrLen < headerLen)
            return EINVAL;

        const SockAddrUn* sa = static_cast<const SockAddrUn*>(addr);
        if (sa->family != UNIX_ADDRESS_FAMILY)
            return EAFNOSUPPORT;

        size_t maxLen = MIN(addrLen - headerLen, UNIX_PATH_MAX);
        size_t len = 0;
        while (len < maxLen && sa->path[len] != '\0')
            len++;

        if (len == 0)
            return EINVAL;

        memcpy(pathOut, sa->path, len);
        pathOut[len] = '\0';
        *pathLenOut = len;
        return 0;
    }

    static void FillSockAddr(void* out, size_t cap, const char* path, size_t pathLen, size_t* realLen) {
        SockAddrUn tmp;
        memset(&tmp, 0, sizeof(SockAddrUn));
        tmp.family = UNIX_ADDRESS_FAMILY;
        size_t total = offsetof(SockAddrUn, path);
        if (path != nullptr && pathLen > 0) {
            memcpy(tmp.path, path, pathLen);
            total += pathLen + 1;
        }
        memcpy(out, &tmp, MIN(cap, total));
        *realLen = total;
    }

    UnixSocket::UnixSocket(SocketType type) : Socket(SocketFamily::Unix, type), m_state(State::Unbound), m_conn(nullptr), m_side(0), m_vnode(nullptr), m_pathLen(0), m_backlogHead(nullptr), m_backlogTail(nullptr), m_backlogCount(0), m_backlogMax(0) {
        m_path[0] = '\0';
        memset(&m_listenerCred, 0, sizeof(m_listenerCred));
    }

    UnixSocket::~UnixSocket() {
        Close();
        if (m_conn != nullptr) {
            m_conn->Unref();
            m_conn = nullptr;
        }
    }

    int UnixSocket::CreatePair(SocketType type, const PeerCred& creator, UnixSocket** a, UnixSocket** b) {
        if (type != SocketType::Stream && type != SocketType::SeqPacket)
            return EPROTOTYPE; // datagram not implemented yet

        UnixConnection* conn = new UnixConnection();
        UnixSocket* sa = new UnixSocket(type);
        UnixSocket* sb = new UnixSocket(type);
        if (conn == nullptr || sa == nullptr || sb == nullptr) {
            delete conn;
            delete sa;
            delete sb;
            return ENOMEM;
        }

        conn->cred[0] = creator;
        conn->cred[1] = creator;

        sa->m_conn = conn;
        sa->m_side = 0;
        sa->m_state = State::Connected;

        conn->Ref();
        sb->m_conn = conn;
        sb->m_side = 1;
        sb->m_state = State::Connected;

        *a = sa;
        *b = sb;
        return 0;
    }

    int UnixSocket::Bind(const void* addr, size_t addrLen, const CallerInfo& caller) {
        char path[UNIX_PATH_MAX + 1];
        size_t pathLen;
        int rc = ParseSockAddr(addr,  addrLen, path, &pathLen);
        if (rc != 0)
            return rc;

        m_lock.Lock();
        if (m_state == State::Closed) {
            m_lock.Unlock();
            return EBADF;
        }
        if (m_state != State::Unbound) {
            m_lock.Unlock();
            return EINVAL;
        }
        m_state = State::Binding;

        m_lock.Unlock();

        char parent[UNIX_PATH_MAX + 1];
        const char* name;
        const char* slash = strrchr(path, '/');
        if (slash == nullptr ) {
            strcpy(parent, ".");
            name = path;
        } else if (slash == path) {
            strcpy(parent, "/");
            name = path + 1;
        } else {
            size_t parentLen = (size_t)(slash - path);
            memcpy(parent, path, parentLen);
            parent[parentLen] = '\0';
            name = slash + 1;
        }

        FS::VNode* vnode = nullptr;
        if (*name == '\0')
            rc = EINVAL; // path ended in a slash
        else {
            mode_t mode = 0777 & ~(mode_t)caller.umask;
            rc = FS::VFS_Create(parent, name, FS::VType::SOCK, mode, caller.cwd, *caller.cred, nullptr, &vnode); // TODO: ref count of vnode on return?
            if (rc < 0)
                rc = -rc;
            if (rc == EEXIST)
                rc = EADDRINUSE;
        }

        if (rc != 0) {
            m_lock.Lock();
            m_state = State::Unbound;
            m_lock.Unlock();
            return rc;
        }

        vnode->Lock();
        vnode->SetBoundSocket(this);
        vnode->Unlock();

        m_lock.Lock();
        m_vnode = vnode;
        memcpy(m_path, path, pathLen + 1);
        m_pathLen = pathLen;
        m_state = State::Bound;
        m_lock.Unlock();
        return 0;
    }

    int UnixSocket::Listen(int backlog, const CallerInfo& caller) {
        if (m_type != SocketType::Stream && m_type != SocketType::SeqPacket)
            return EOPNOTSUPP;

        m_lock.Lock();
        if (m_state != State::Bound && m_state != State::Listening) {
            State s = m_state;
            m_lock.Unlock();
            return s == State::Closed ? EBADF : EINVAL;
        }

        size_t max = backlog < 1 ? 1 : (size_t)backlog;
        m_backlogMax = MIN(max, MAX_BACKLOG);
        m_listenerCred = caller.peer;
        m_state = State::Listening;
        m_lock.Unlock();

        m_listenQueue.Trigger(ALL_EVENTS);
        return 0;
    }

    int UnixSocket::Accept(Socket** out, bool nonBlock) {
        m_lock.Lock();
        while (true) {
            if (m_state != State::Listening) {
                m_lock.Unlock();
                return EINVAL;
            }
            if (m_backlogHead != nullptr)
                break;
            if (nonBlock) {
                m_lock.Unlock();
                return EAGAIN;
            }
            
            if (0 != WaitOnQueue(m_listenQueue, m_lock)) {
                m_lock.Unlock();
                return EINTR;
            }
        }

        UnixConnection* conn = m_backlogHead;
        m_backlogHead = conn->pendingNext;
        if (m_backlogHead == nullptr)
            m_backlogTail = nullptr;
        conn->pendingNext = nullptr;
        m_backlogCount--;
        m_lock.Unlock();

        m_listenQueue.Trigger(ALL_EVENTS);

        UnixSocket* accepted = new UnixSocket(m_type);
        if (accepted == nullptr) {
            ShutdownSide(conn, 1, true, true);
            conn->Unref();
            return ENOMEM;
        }

        accepted->m_conn = conn;
        accepted->m_side = 1;
        accepted->m_state = State::Connected;

        *out = accepted;
        return 0;
    }

    int UnixSocket::Connect(const void* addr, size_t addrLen, const CallerInfo& caller, bool nonBlock) {
        char path[UNIX_PATH_MAX + 1];
        size_t pathLen;
        int rc = ParseSockAddr(addr, addrLen, path, &pathLen);
        if (rc != 0)
            return rc;

        m_lock.Lock();
        State prev = m_state;
        switch (prev) {
        case State::Unbound:
        case State::Bound:
            m_state = State::Connecting;
            m_lock.Unlock();
            break;
        case State::Connected:
            m_lock.Unlock();
            return EISCONN;
        case State::Connecting:
        case State::Binding:
            m_lock.Unlock();
            return EALREADY;
        case State::Listening:
            m_lock.Unlock();
            return EINVAL;
        default:
            m_lock.Unlock();
            return EBADF;
        }

        auto restore = [&](int err) {
            m_lock.Lock();
            m_state = prev;
            m_lock.Unlock();
            return err;
        };

        FS::VNode* vnode = nullptr;
        FS::VFS* vfs = nullptr;
        rc = FS::VFS_LookupPath(path, &vnode, &vfs, caller.cwd, *caller.cred, true);
        if (rc != 0)
            return restore(-rc);

        if (vnode->GetType() != FS::VType::SOCK) {
            FS::UnrefVNode(vnode);
            return restore(ECONNREFUSED);
        }
        rc = vnode->Access(ACCESS_W, *caller.cred);
        if (rc != 0) {
            FS::UnrefVNode(vnode);
            return restore(-rc);
        }

        UnixSocket* listener = nullptr;
        vnode->Lock();
        Socket* bound = vnode->GetBoundSocket();
        if (bound != nullptr && bound->GetFamily() == SocketFamily::Unix) {
            listener = static_cast<UnixSocket*>(bound);
            listener->Ref();
        }
        vnode->Unlock();
        FS::UnrefVNode(vnode);

        if (listener == nullptr)
            return restore(ECONNREFUSED);
        if (listener->m_type != m_type) {
            listener->Unref();
            return restore(EPROTOTYPE);
        }

        UnixConnection* conn = new UnixConnection();
        if (conn == nullptr) {
            listener->Unref();
            return restore(ENOMEM);
        }
        conn->cred[0] = caller.peer;

        rc = listener->EnqueueIncoming(conn, nonBlock);
        listener->Unref();
        if (rc != 0) {
            conn->Unref();
            return restore(rc);
        }

        m_lock.Lock();
        m_conn = conn;
        m_side = 0;
        m_state = State::Connected;
        m_lock.Unlock();
        return 0;
    }

    int UnixSocket::Send(const SendParams& p, size_t* sent) {
        *sent = 0;

        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;

        const bool stream = (m_type == SocketType::Stream);

        if (p.fdCount > MAX_FDS_PER_MESSAGE)
            return EINVAL;
        if (stream) {
            if (p.length == 0 && p.fdCount == 0)
                return 0;
            if (p.length == 0)
                return EINVAL;
        } else if (MessageCharge(p.fdCount, p.length) > DIRECTION_LIMIT)
            return EMSGSIZE;

        UnixDirection& dir = conn->dir[side];
        const uint8_t* src = static_cast<const uint8_t*>(p.data);
        size_t remaining = p.length;
        bool first = true;
        UnixMessage* pending = nullptr;
        int err = 0;

        do {
            size_t chunk = stream ? MIN(remaining, STREAM_MAX_ENTRY) : remaining;
            size_t slots = first ? p.fdCount : 0;
            bool sealed = !stream || slots > 0;
            size_t capacity = sealed ? chunk : MAX(chunk, STREAM_CHUNK);
            size_t charge = MessageCharge(slots, capacity);
            size_t consumed = 0;
            bool placed = false;

            dir.lock.Lock();
            while (true) {
                if (dir.readerDone || dir.writerDone) {
                    err = EPIPE;
                    break;
                }

                if (!pending && stream && slots == 0 && dir.tail && !dir.tail->sealed && dir.tail->capacity > dir.tail->length) {
                    UnixMessage* tail = dir.tail;
                    size_t n = MIN(chunk, tail->capacity - tail->length);
                    memcpy(tail->Data() + tail->length, src, n);
                    tail->length += n;
                    dir.readable += n;
                    consumed = n;
                    placed = true;
                    break;
                }

                if (pending) {
                    if (dir.charged + pending->Charge() <= DIRECTION_LIMIT) {
                        if (dir.tail)
                            dir.tail->next = pending;
                        else
                            dir.head = pending;
                        dir.tail = pending;
                        dir.charged += pending->Charge();
                        dir.readable += pending->length;
                        dir.messages++;
                        consumed = chunk;
                        pending = nullptr;
                        placed = true;
                        break;
                    }
                } else if (dir.charged + charge <= DIRECTION_LIMIT) {
                    dir.lock.Unlock();
                    pending = AllocMessage(slots, capacity);
                    if (pending != nullptr) {
                        memcpy(pending->Data(), src, chunk);
                        pending->length = chunk;
                        pending->sealed = sealed;
                        for (size_t i = 0; i < slots; i++) {
                            p.fds[i]->Ref();
                            pending->Fds()[i] = p.fds[i];
                        }
                        pending->fdCount = (uint32_t)slots;
                    }
                    dir.lock.Lock();
                    if (pending == nullptr) {
                        err = ENOMEM;
                        break;
                    }
                    continue;
                }

                // No room: block until there is room
                if (p.nonBlock) {
                    err = EAGAIN;
                    break;
                }
                if (0 != WaitOnQueue(conn->waitq[side], dir.lock)) {
                    err = EINTR;
                    break;
                }
            }
            dir.lock.Unlock();

            if (!placed)
                break;

            conn->waitq[1 - side].Trigger(ALL_EVENTS);
            *sent += consumed;
            src += consumed;
            remaining -= consumed;
            first = false;
        } while(remaining > 0);

        if (pending != nullptr)
            FreeMessageList(pending);

        if (err == EPIPE && !p.noSignal)
            Thread::GetCurrentThread()->RaiseSignal(SIGPIPE);

        if (*sent > 0)
            return 0;
        return err;
    }

    int UnixSocket::Recv(const RecvParams& p, RecvResult* r) {
        r->bytes = 0;
        r->fdCount = 0;
        r->truncated = false;
        r->controlTruncated = false;

        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;

        const bool stream = (m_type == SocketType::Stream);
        UnixDirection& in = conn->dir[1 - side];

        UnixMessage* freeList = nullptr;
        UnixMessage* freeTail = nullptr;
        FileDescriptor* dropped[MAX_FDS_PER_MESSAGE];
        size_t droppedCount = 0;
        uint8_t* out = static_cast<uint8_t*>(p.buffer);
        size_t total = 0;

        in.lock.Lock();
        while (true) {
            if (in.head != nullptr)
                break;
            if (in.writerDone || in.readerDone) {
                in.lock.Unlock();
                return 0;
            }
            if (p.nonBlock) {
                in.lock.Unlock();
                return EAGAIN;
            }
            if (0 != WaitOnQueue(conn->waitq[side], in.lock)) {
                in.lock.Unlock();
                return EINTR;
            }
        }

        auto deliverFds = [&](UnixMessage* m) {
            for (uint32_t i = 0; i < m->fdCount; i++) {
                if (r->fdCount < p.fdCapacity)
                    p.fdsOut[r->fdCount++] = m->Fds()[i];
                else
                    dropped[droppedCount++] = m->Fds()[i];
            }
            if (droppedCount > 0)
                r->controlTruncated = true;
            m->fdCount = 0;
        };

        auto unlinkHead = [&]() {
            UnixMessage* m = in.head;
            in.head = m->next;
            if (in.head == nullptr)
                in.tail = nullptr;
            in.charged -= m->Charge();
            in.messages--;
            m->next = nullptr;
            if (freeTail != nullptr)
                freeTail->next = m;
            else
                freeList = m;
            freeTail = m;
        };

        if (!stream) {
            UnixMessage* m = in.head;
            deliverFds(m);
            size_t n = MIN(p.length, m->Remaining());
            memcpy(out, m->Data() + m->readOffset, n);
            if (m->Remaining() > n)
                r->truncated = true;
            in.readable -= m->Remaining();
            total = n;
            unlinkHead();
        } else {
            while (in.head && total < p.length) {
                UnixMessage* m = in.head;
                if (m->fdCount > 0) {
                    if (total > 0)
                        break;
                    deliverFds(m);
                }
                size_t n = MIN(p.length - total, m->Remaining());
                memcpy(out + total, m->Data() + m->readOffset, n);
                m->readOffset += n;
                in.readable -= n;
                total += n;
                if (m->Remaining() == 0)
                    unlinkHead();
            }
        }
        in.lock.Unlock();

        FreeMessageList(freeList);
        for (size_t i = 0; i < droppedCount; i++)
            dropped[i]->Unref();

        conn->waitq[1 - side].Trigger(ALL_EVENTS);

        r->bytes = total;
        return 0;
    }

    int UnixSocket::Shutdown(ShutdownHow how) {
        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;

        ShutdownSide(conn, side, how != ShutdownHow::Write, how != ShutdownHow::Read);
        return 0;
    }

    int UnixSocket::Close() {
        m_lock.Lock();
        if (m_state == State::Closed) {
            m_lock.Unlock();
            return 0;
        }
        State prev = m_state;
        m_state = State::Closed;
        UnixConnection* conn = m_conn;
        int side = m_side;
        FS::VNode* vnode = m_vnode;
        m_vnode = nullptr;
        UnixConnection* backlog = m_backlogHead;
        m_backlogHead = nullptr;
        m_backlogTail = nullptr;
        m_backlogCount = 0;
        m_lock.Unlock();

        if (vnode != nullptr) {
            vnode->Lock();
            if (vnode->GetBoundSocket() == this)
                vnode->SetBoundSocket(nullptr);
            vnode->Unlock();
            FS::UnrefVNode(vnode);
        }

        while (backlog != nullptr) {
            UnixConnection* next = backlog->pendingNext;
            backlog->pendingNext = nullptr;
            ShutdownSide(backlog, 1, true, true);
            backlog->Unref();
            backlog = next;
        }

        if (conn != nullptr && prev == State::Connected)
            ShutdownSide(conn, side, true, true);

        m_listenQueue.Trigger(ALL_EVENTS);
        return 0;
    }

    int UnixSocket::GetSockName(void* out, size_t cap, size_t* realLen) {
        m_lock.Lock();
        FillSockAddr(out, cap, m_pathLen > 0 ? m_path : nullptr, m_pathLen, realLen);
        m_lock.Unlock();
        return 0;
    }

    int UnixSocket::GetPeerName(void* out, size_t cap, size_t* realLen) {
        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;
        FillSockAddr(out, cap, nullptr, 0, realLen); // TODO: store the listener's path
        return 0;
    }

    int UnixSocket::GetPeerCred(PeerCred* out) {
        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;
        *out = conn->cred[1 - side];
        return 0;
    }

    int UnixSocket::Ioctl(uint64_t op, void* arg, int* result, Process* currentProc) {
        if (op != FIONREAD)
            return ENOTTY;
        UnixConnection* conn;
        int side;
        int rc = GetConnection(&conn, &side);
        if (rc != 0)
            return rc;
        UnixDirection& in = conn->dir[1 - side];
        in.lock.Lock();
        int avail = (int)(m_type == SocketType::SeqPacket ? (in.head != nullptr ? in.head->Remaining() : 0) : in.readable);
        in.lock.Unlock();
        if (!UserWrite(arg, &avail, sizeof(avail), currentProc))
            return EFAULT;
        *result = 0;
        return 0;
    }

    EventWaitQueue* UnixSocket::GetPollQueue(uint32_t) {
        m_lock.Lock();
        EventWaitQueue* q = (m_state == State::Connected) ? &m_conn->waitq[m_side] : &m_listenQueue;
        m_lock.Unlock();
        return q;
    }

    uint32_t UnixSocket::PollReady(uint32_t events) {
        m_lock.Lock();
        State state = m_state;
        UnixConnection* conn = m_conn;
        int side = m_side;
        size_t pendingConnections = m_backlogCount;
        m_lock.Unlock();

        uint32_t mask = 0;
        
        if (state == State::Listening) {
            if (pendingConnections > 0)
                mask |= POLLIN | POLLRDNORM;
        } else if (state == State::Connected) {
            UnixDirection& in = conn->dir[1 - side];
            UnixDirection& out = conn->dir[side];

            in.lock.Lock();
            bool readable = in.head != nullptr;
            bool recvDone = in.writerDone || in.readerDone;
            bool peerGone = in.writerDone;
            in.lock.Unlock();

            out.lock.Lock();
            bool sendDone = out.readerDone || out.writerDone;
            bool hasSpace;
            if (m_type == SocketType::Stream)
                hasSpace = (out.tail != nullptr && !out.tail->sealed && out.tail->capacity > out.tail->length) || out.charged + MessageCharge(0, STREAM_CHUNK) <= DIRECTION_LIMIT;
            else
                hasSpace = out.charged + MessageCharge(0, 1) <= DIRECTION_LIMIT;
            out.lock.Unlock();

            if (readable || recvDone)
                mask |= POLLIN | POLLRDNORM;
            if (peerGone)
                mask |= POLLRDHUP;
            if (sendDone || hasSpace)
                mask |= POLLOUT | POLLWRNORM;
            if (recvDone && sendDone)
                mask |= POLLHUP;
        } else if (state == State::Closed)
            mask |= POLLHUP | POLLERR;
        else
            mask |= POLLOUT | POLLWRNORM;

        return mask & (events | POLLHUP | POLLERR);
    }

    int UnixSocket::GetConnection(UnixConnection** conn, int* side) {
        m_lock.Lock();
        State state = m_state;
        *conn = m_conn;
        *side = m_side;
        m_lock.Unlock();

        if (state == State::Connected)
            return 0;
        return state == State::Closed ? EBADF : ENOTCONN;
    }

    int UnixSocket::EnqueueIncoming(UnixConnection* conn, bool nonBlock) {
        m_lock.Lock();
        while (true) {
            if (m_state != State::Listening) {
                m_lock.Unlock();
                return ECONNREFUSED;
            }
            if (m_backlogCount < m_backlogMax)
                break;
            if (nonBlock) {
                m_lock.Unlock();
                return EAGAIN;
            }

            if (0 != WaitOnQueue(m_listenQueue, m_lock)) {
                m_lock.Unlock();
                return EINTR;
            }
        }

        conn->cred[1] = m_listenerCred;
        conn->Ref();
        conn->pendingNext = nullptr;
        if (m_backlogTail != nullptr)
            m_backlogTail->pendingNext = conn;
        else
            m_backlogHead = conn;
        m_backlogTail = conn;
        m_backlogCount++;
        m_lock.Unlock();

        m_listenQueue.Trigger(ALL_EVENTS);
        return 0;
    }

}