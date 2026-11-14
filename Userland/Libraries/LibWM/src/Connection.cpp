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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <deque>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <LibWM/Connection.hpp>
#include <LibWM/Message.hpp>

namespace LibWM {

    static bool FillAddr(sockaddr_un& addr, const char* path) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        size_t n = strlen(path);
        if (n == 0 || n >= sizeof(addr.sun_path)) {
            errno = ENAMETOOLONG;
            return false;
        }
        memcpy(addr.sun_path, path, n);
        return true;
    }

    Connection Connection::Connect(const char* path) {
        sockaddr_un addr;
        if (!FillAddr(addr, path))
            return Connection();

        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
            return Connection();

        int r;
        do {
            r = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        } while (r < 0 && errno == EINTR);

        if (r < 0) {
            int e = errno;
            close(fd);
            errno = e;
            return Connection();
        }
        return Connection(fd);
    }

    void Connection::Close() {
        if (m_fd >= 0)
            close(m_fd);
        m_fd = -1;
        for (Chunk& c : m_out) {
            for (int f : c.fds)
                close(f);
        }
        m_out.clear();
        m_outBytes = 0;
        for (int f : m_inFDs)
            close(f);
        m_inFDs.clear();
        m_inPos = 0;
        m_inEnd = 0;
    }

    bool Connection::SetNonBlocking(bool on) {
        int fl = fcntl(m_fd, F_GETFL);
        if (fl < 0)
            return false;
        fl = on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK);
        return fcntl(m_fd, F_SETFL, fl) == 0;
    }

    bool Connection::PeerCred(pid_t& pid, uid_t& uid, gid_t& gid) const {
        ucred cred;
        socklen_t len = sizeof(cred);
        if (getsockopt(m_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
            return false;
        pid = cred.pid;
        uid = cred.uid;
        gid = cred.gid;
        return true;
    }

    bool Connection::Queue(uint32_t object, uint16_t opcode, const void* payload, size_t len, const int* fds, size_t nfds) {
        if (m_fd < 0)
            return false;

        size_t padded = (len + 3) & ~size_t(3);
        size_t total = sizeof(MessageHeader) + padded;
        if (total > MaxMessageSize || nfds > MaxFdsPerMessage)
            return false;
        if (m_outBytes + total > MaxOutputBacklog)
            return false;

        Chunk c;
        c.bytes.assign(total, 0);
        MessageHeader h{object, opcode, static_cast<uint16_t>(total)};
        memcpy(c.bytes.data(), &h, sizeof(h));
        if (len > 0)
            memcpy(c.bytes.data() + sizeof(h), payload, len);

        for (size_t i = 0; i < nfds; i++) {
            int d = fcntl(fds[i], F_DUPFD_CLOEXEC, 0);
            if (d < 0) {
                for (int f : c.fds)
                    close(f);
                return false;
            }
            c.fds.push_back(d);
        }

        m_outBytes += total;
        m_out.push_back(std::move(c));
        return true;
    }

    Status Connection::Flush() {
        if (m_fd < 0)
            return Status::Closed;

        while (!m_out.empty()) {
            Chunk& c = m_out.front();

            iovec iov;
            iov.iov_base = c.bytes.data() + c.sent;
            iov.iov_len = c.bytes.size() - c.sent;

            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;

            alignas(cmsghdr) char ctl[CMSG_SPACE(sizeof(int) * MaxFdsPerMessage)];
            if (!c.fds.empty()) {
                size_t n = c.fds.size();
                memset(ctl, 0, sizeof(ctl));
                msg.msg_control = ctl;
                msg.msg_controllen = CMSG_SPACE(sizeof(int) * n);
                cmsghdr* cm = CMSG_FIRSTHDR(&msg);
                cm->cmsg_level = SOL_SOCKET;
                cm->cmsg_type = SCM_RIGHTS;
                cm->cmsg_len = CMSG_LEN(sizeof(int) * n);
                memcpy(CMSG_DATA(cm), c.fds.data(), sizeof(int) * n);
            }

            ssize_t n;
            do {
                n = sendmsg(m_fd, &msg, MSG_NOSIGNAL);
            } while (n < 0 && errno == EINTR);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return Status::WouldBlock;
                if (errno == EPIPE || errno == ECONNRESET)
                    return Status::Closed;
                return Status::Error;
            }

            for (int f : c.fds)
                close(f);

            c.fds.clear();

            c.sent += static_cast<size_t>(n);
            m_outBytes -= static_cast<size_t>(n);
            if (c.sent == c.bytes.size())
                m_out.pop_front();
        }
        return Status::Ok;
    }

    Status Connection::Fill() {
        if (m_fd < 0)
            return Status::Closed;

        if (m_inPos > 0) {
            memmove(m_in.data(), m_in.data() + m_inPos, m_inEnd - m_inPos);
            m_inEnd -= m_inPos;
            m_inPos = 0;
        }
        if (m_inEnd == m_in.size())
            return Status::Ok;

        iovec iov;
        iov.iov_base = m_in.data() + m_inEnd;
        iov.iov_len = m_in.size() - m_inEnd;

        alignas(cmsghdr) char ctl[CMSG_SPACE(sizeof(int) * MaxFdsPerMessage * 4)];
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ctl;
        msg.msg_controllen = sizeof(ctl);

        ssize_t n;
        do {
            n = recvmsg(m_fd, &msg, MSG_CMSG_CLOEXEC);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return Status::WouldBlock;
            if (errno == EPIPE || errno == ECONNRESET)
                return Status::Closed;
            return Status::Error;
        }

        for (cmsghdr* cm = CMSG_FIRSTHDR(&msg); cm != nullptr; cm = CMSG_NXTHDR(&msg, cm)) {
            if (cm->cmsg_level != SOL_SOCKET || cm->cmsg_type != SCM_RIGHTS)
                continue;

            size_t count = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int* data = reinterpret_cast<int*>(CMSG_DATA(cm));
            for (size_t i = 0; i < count; i++) {
                if (m_inFDs.size() >= MaxPendingInputFds) {
                    close(data[i]);
                    m_protoError = true;
                } else
                    m_inFDs.push_back(data[i]);
            }
        }

        if ((msg.msg_flags & MSG_CTRUNC) > 0)
            m_protoError = true;
        if (m_protoError)
            return Status::Protocol;

        if (n == 0)
            return Status::Closed;
        m_inEnd += static_cast<size_t>(n);
        return Status::Ok;
    }

    bool Connection::Next(Message& out) {
        size_t avail = m_inEnd - m_inPos;
        if (avail < sizeof(MessageHeader))
            return false;

        MessageHeader h;
        memcpy(&h, m_in.data() + m_inPos, sizeof(h));

        if (h.size < sizeof(MessageHeader) || h.size % 4 != 0 || h.size > MaxMessageSize) {
            m_protoError = true;
            return false;
        }
        if (avail < h.size)
            return false;

        out.header = h;
        out.payload = m_in.data() + m_inPos + sizeof(h);
        out.payloadSize = h.size - sizeof(h);
        m_inPos += h.size;
        return true;
    }

    int Connection::TakeFD() {
        if (m_inFDs.empty())
            return -1;
        int f = m_inFDs.front();
        m_inFDs.pop_front();
        return f;
    }

    void Connection::Take(Connection& o) {
        m_fd = o.m_fd;
        o.m_fd = -1;

        m_out = std::move(o.m_out);
        o.m_out.clear();

        m_outBytes = o.m_outBytes;
        o.m_outBytes = 0;

        m_in = std::move(o.m_in);
        o.m_in.assign(2 * MaxMessageSize, 0);

        m_inPos = o.m_inPos;
        o.m_inPos = 0;

        m_inEnd = o.m_inEnd;
        o.m_inEnd = 0;

        m_inFDs = std::move(o.m_inFDs);
        o.m_inFDs.clear();

        m_protoError = o.m_protoError;
        o.m_protoError = false;
    }


    Listener::~Listener() {
        if (m_fd >= 0) {
            close(m_fd);
            if (m_path[0] != 0)
                unlink(m_path);
        }
    }

    bool Listener::Listen(const char* path, mode_t mode) {
        sockaddr_un addr;
        if (!FillAddr(addr, path))
            return false;

        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0)
            return false;

        unlink(path);

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || chmod(path, mode) < 0 || listen(fd, 32) < 0) {
            int e = errno;
            close(fd);
            errno = e;
            return false;
        }

        m_fd = fd;
        strncpy(m_path, path, sizeof(m_path) - 1);
        return true;
    }

    Connection Listener::Accept() {
        int fd;
        do {
            fd = accept4(m_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
        } while (fd < 0 && errno == EINTR);
        return fd < 0 ? Connection() : Connection(fd);
    }

}