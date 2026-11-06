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

#include "File.hpp"
#include "Socket.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <util.h>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <Net/Socket.hpp>
#include <Net/UnixSocket.hpp>

#include <Scheduling/Process.hpp>

namespace {
    constexpr size_t SOCKET_IO_MAX = 65536;
    constexpr size_t SOCKET_CONTROL_MAX = 4096;
    constexpr int REPORTED_BUFFER_SIZE = 65536;

    Net::PeerCred MakePeerCred(Process* proc) {
        Credential c = proc->GetCred();
        return {
            static_cast<int32_t>(proc->GetPID()),
            static_cast<int32_t>(c.euid),
            static_cast<int32_t>(c.egid)
        };
    }

    Net::SocketType GetSockType(int type) {
        return type == SOCK_SEQPACKET ? Net::SocketType::SeqPacket : Net::SocketType::Stream;
    }

    struct KernelBuffer {
        void* ptr = nullptr;
        KernelBuffer() = default;
        KernelBuffer(const KernelBuffer&) = delete;
        KernelBuffer& operator=(const KernelBuffer&) = delete;
        ~KernelBuffer() {
            if (ptr != nullptr)
                kfree(ptr);
        }
        bool Alloc(size_t size) {
            ptr = kcalloc(1, size != 0 ? size : 1);
            return ptr != nullptr;
        }
        template <typename T>
        T* As() {
            return static_cast<T*>(ptr);
        }
    };

    struct Caller {
        Credential cred;
        FS::VNode* cwd;
        Net::CallerInfo info;

        explicit Caller(Process* proc) : cred(proc->GetCred()), cwd(proc->GetCWDRef()), info{&cred, cwd, static_cast<uint32_t>(proc->GetUMask()), MakePeerCred(proc)} {}
        ~Caller() {
            FS::UnrefVNode(cwd);
        }
        Caller(const Caller&) = delete;
        Caller& operator=(const Caller&) = delete;
    };

    struct SocketRef {
        Net::Socket* sock = nullptr;
        bool nonBlock = false;
        int err = 0;

        SocketRef(Process* proc, int fd) {
            FileDescriptorManager* mgr = proc->GetFDManager();
            if (mgr == nullptr) {
                err = ENOSYS;
                return;
            }
            FileDescriptor* desc = mgr->Get(fd);
            if (desc == nullptr) {
                err = EBADF;
                return;
            }
            if (!desc->isOpen())
                err = EBADF;
            else if (desc->GetType() != FDType::Socket || desc->GetSocket() == nullptr)
                err = ENOTSOCK;
            else {
                sock = desc->GetSocket();
                sock->Ref();
                nonBlock = (desc->GetFlags() & O_NONBLOCK) > 0;
            }
            desc->Unref();
        }
        ~SocketRef() {
            if (sock != nullptr)
                sock->Unref();
        }
        SocketRef(const SocketRef&) = delete;
        SocketRef& operator=(const SocketRef&) = delete;
    };

    struct FDRefList {
        FileDescriptor* fds[Net::MAX_FDS_PER_MESSAGE];
        size_t count = 0;
        ~FDRefList() {
            for (size_t i = 0; i < count; i++)
                fds[i]->Unref();
        }
    };

    fd_t InstallSocket(Process* proc, Net::Socket* sock, int flags) {
        FileDescriptorManager* mgr = proc->GetFDManager();
        if (mgr == nullptr) {
            sock->Close();
            sock->Unref();
            return -ENOSYS;
        }

        FileDescriptor* desc = new FileDescriptor(proc, sock);
        int rc = desc->Open(O_RDWR | (flags & O_NONBLOCK));
        if (rc < 0) {
            desc->Unref();
            return rc;
        }

        fd_t fd = mgr->Allocate(desc, (flags & O_CLOEXEC) > 0 ? FD_CLOEXEC : 0);
        if (fd < 0)
            desc->Unref();
        return fd;
    }

    int WriteSockAddr(Process* proc, sockaddr* addr, socklen_t maxLen, socklen_t* actualLen, const void* kaddr, size_t realLen) {
        socklen_t out = static_cast<socklen_t>(realLen);
        size_t copy = MIN(static_cast<size_t>(maxLen), realLen);
        if (copy > 0 && !UserWrite(addr, kaddr, copy, proc))
            return EFAULT;
        if (!UserWrite(actualLen, &out, sizeof(out), proc))
            return EFAULT;
        return 0;
    }

    int SockNameImpl(int fd, sockaddr* addr, socklen_t maxLen, socklen_t* actualLen, bool peer) {
        if (addr == nullptr || actualLen == nullptr)
            return addr == nullptr ? EFAULT : EINVAL;

        Process* proc = Thread::GetCurrentThread()->GetParent();
        SocketRef ref(proc, fd);
        if (ref.sock == nullptr)
            return ref.err;

        uint8_t kaddr[sizeof(Net::SockAddrUn)] = {};
        size_t realLen = 0;
        int rc = peer ? ref.sock->GetPeerName(kaddr, sizeof(kaddr), &realLen) : ref.sock->GetSockName(kaddr, sizeof(kaddr), &realLen);
        if (rc != 0)
            return rc;
        return WriteSockAddr(proc, addr, maxLen, actualLen, kaddr, realLen);
    }

    int LoadIovecs(Process* proc, const msghdr& kh, KernelBuffer& store, const iovec** vec, size_t* total) {
        *vec = nullptr;
        *total = 0;

        size_t count = kh.iovcount;
        if (count > UIO_MAXIOV)
            return EMSGSIZE;
        if (count == 0)
            return 0;
        if (kh.iov == nullptr)
            return EFAULT;
        if (!store.Alloc(count * sizeof(iovec)))
            return ENOMEM;
        if (!UserRead(kh.iov, store.ptr, count * sizeof(iovec), proc))
            return EFAULT;

        const iovec* v = store.As<iovec>();
        size_t sum = 0;
        for (size_t i = 0; i < count; i++) {
            size_t len = v[i].iovLen;
            sum = (len > SOCKET_IO_MAX) ? SOCKET_IO_MAX + 1 : MIN(sum + len, SOCKET_IO_MAX + 1);
        }

        *vec = v;
        *total = sum;
        return 0;
    }

    int ParseControlFDs(Process* proc, const msghdr& kh, FDRefList& out) {
        if (kh.ctrllen > SOCKET_CONTROL_MAX)
            return ENOBUFS;

        KernelBuffer ctrl;
        if (!ctrl.Alloc(kh.ctrllen))
            return ENOMEM;
        if (!UserRead(kh.msgctrl, ctrl.ptr, kh.ctrllen, proc))
            return EFAULT;

        FileDescriptorManager* mgr = proc->GetFDManager();
        if (mgr == nullptr)
            return ENOSYS;

        const uint8_t* base = ctrl.As<uint8_t>();
        size_t off = 0;
        while (off + sizeof(cmsghdr) <= kh.ctrllen) {
            cmsghdr hdr;
            memcpy(&hdr, base + off, sizeof(hdr));
            if (hdr.cmsgLen < CMSG_LEN(0) || hdr.cmsgLen > kh.ctrllen - off)
                return EINVAL;
            if (hdr.cmsgLevel != SOL_SOCKET || hdr.cmsgType != SCM_RIGHTS)
                return EINVAL;

            size_t dataLen = hdr.cmsgLen - CMSG_LEN(0);
            if (dataLen % sizeof(int) != 0)
                return EINVAL;
            size_t n = dataLen / sizeof(int);
            if (n > Net::MAX_FDS_PER_MESSAGE - out.count)
                return EINVAL;

            for (size_t i = 0; i < n; i++) {
                int num;
                memcpy(&num, base + off + CMSG_LEN(0) + i * sizeof(int), sizeof(int));
                FileDescriptor* desc = mgr->Get(num);
                if (desc == nullptr)
                    return EBADF;
                if (!desc->isOpen()) {
                    desc->Unref();
                    return EBADF;
                }
                out.fds[out.count++] = desc;
            }

            off += CMSG_ALIGN(hdr.cmsgLen);
        }
        return 0;
    }
}

int sys_socket(int domain, int typeFlags, int protocol) {
    int flags = typeFlags & (SOCK_NONBLOCK | SOCK_CLOEXEC);
    int type = typeFlags & ~flags;
    if (domain != AF_UNIX)
        return -EAFNOSUPPORT;
    if ((type != SOCK_STREAM && type != SOCK_SEQPACKET) || protocol != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();

    Net::UnixSocket* sock = new Net::UnixSocket(GetSockType(type));
    return static_cast<int>(InstallSocket(proc, sock, flags));
}

int sys_socketpair(int domain, int typeFlags, int protocol, int fds[2]) {
    int flags = typeFlags & (SOCK_NONBLOCK | SOCK_CLOEXEC);
    int type = typeFlags & ~flags;
    if (domain != AF_UNIX)
        return EAFNOSUPPORT;
    if ((type != SOCK_STREAM && type != SOCK_SEQPACKET) || protocol != 0)
        return EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    FileDescriptorManager* mgr = proc->GetFDManager();
    if (mgr == nullptr)
        return ENOSYS;

    Net::UnixSocket* socks[2];
    int rc = Net::UnixSocket::CreatePair(GetSockType(type), MakePeerCred(proc), &socks[0], &socks[1]);
    if (rc != 0)
        return rc;

    fd_t a = InstallSocket(proc, socks[0], flags);
    if (a < 0) {
        socks[1]->Close();
        socks[1]->Unref();
        return static_cast<int>(-a);
    }

    fd_t b = InstallSocket(proc, socks[1], flags);
    if (b < 0) {
        mgr->Free(a);
        return static_cast<int>(-b);
    }

    int kfds[2] = {static_cast<int>(a), static_cast<int>(b)};
    if (!UserWrite(fds, kfds, sizeof(kfds), proc)) {
        mgr->Free(a);
        mgr->Free(b);
        return EFAULT;
    }
    return 0;
}

int sys_bind(int fd, const sockaddr* addr, socklen_t addrLen) {
    if (addr == nullptr || addrLen < sizeof(uint16_t) || addrLen > sizeof(Net::SockAddrUn))
        return addr == nullptr ? EFAULT : EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    uint8_t kaddr[sizeof(Net::SockAddrUn)] = {};
    if (!UserRead(addr, kaddr, addrLen, proc))
        return EFAULT;

    Caller caller(proc);
    return ref.sock->Bind(kaddr, addrLen, caller.info);
}

int sys_listen(int fd, int backlog) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    Caller caller(proc);
    return ref.sock->Listen(backlog, caller.info);
}

int sys_accept(int fd, sockaddr* addr, socklen_t maxAddrLen, socklen_t* actualLen, int flags) {
    if ((flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) != 0)
        return -EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return -ref.err;

    Net::Socket* newSock = nullptr;
    int rc = ref.sock->Accept(&newSock, ref.nonBlock);
    if (rc != 0)
        return -rc;

    if (addr != nullptr && actualLen != nullptr) {
        uint8_t kaddr[sizeof(Net::SockAddrUn)] = {};
        size_t realLen = 0;
        rc = newSock->GetPeerName(kaddr, sizeof(kaddr), &realLen);
        if (rc == 0)
            rc = WriteSockAddr(proc, addr, maxAddrLen, actualLen, kaddr, realLen);
        if (rc != 0) {
            newSock->Close();
            newSock->Unref();
            return -rc;
        }
    }

    return static_cast<int>(InstallSocket(proc, newSock, flags));
}

int sys_connect(int fd, const sockaddr* addr, socklen_t addrLen) {
    if (addr == nullptr || addrLen < sizeof(uint16_t) || addrLen > sizeof(Net::SockAddrUn))
        return addr == nullptr ? EFAULT : EINVAL;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    uint8_t kaddr[sizeof(Net::SockAddrUn)] = {};
    if (!UserRead(addr, kaddr, addrLen, proc))
        return EFAULT;

    Caller caller(proc);
    return ref.sock->Connect(kaddr, addrLen, caller.info, ref.nonBlock);
}

ssize_t sys_sendmsg(int fd, msghdr* hdr, int flags) {
    if ((flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL)) != 0)
        return -EOPNOTSUPP;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return -ref.err;

    msghdr kh;
    if (hdr == nullptr || !UserRead(hdr, &kh, sizeof(kh), proc))
        return -EFAULT;

    KernelBuffer iovStore;
    const iovec* iov = nullptr;
    size_t sum = 0;
    int rc = LoadIovecs(proc, kh, iovStore, &iov, &sum);
    if (rc != 0)
        return -rc;

    size_t length = sum;
    if (sum > SOCKET_IO_MAX) {
        if (ref.sock->GetType() != Net::SocketType::Stream)
            return -EMSGSIZE; // message can't be split
        length = SOCKET_IO_MAX;
    }

    KernelBuffer data;
    if (!data.Alloc(length))
        return -ENOMEM;

    size_t off = 0;
    for (size_t i = 0; i < kh.iovcount && off < length; i++) {
        size_t n = MIN(iov[i].iovLen, length - off);
        if (n > 0 && !UserRead(iov[i].iovBase, data.As<uint8_t>() + off, n, proc))
            return -EFAULT;
        off += n;
    }

    FDRefList fdRefs;
    if (kh.msgctrl != nullptr && kh.ctrllen > 0) {
        rc = ParseControlFDs(proc, kh, fdRefs);
        if (rc != 0)
            return -rc;
    }

    Net::SendParams params = {data.ptr, length, fdRefs.fds, fdRefs.count, ref.nonBlock || (flags & MSG_DONTWAIT) != 0, (flags & MSG_NOSIGNAL) != 0};
    size_t sent = 0;
    rc = ref.sock->Send(params, &sent);
    if (rc != 0)
        return -rc;
    return static_cast<ssize_t>(sent);
}

ssize_t sys_recvmsg(int fd, msghdr* hdr, int flags) {
     if ((flags & ~(MSG_DONTWAIT | MSG_CMSG_CLOEXEC)) != 0)
        return -EOPNOTSUPP;

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return -ref.err;

    msghdr kh;
    if (hdr == nullptr || !UserRead(hdr, &kh, sizeof(kh), proc))
        return -EFAULT;

    KernelBuffer iovStore;
    const iovec* iov = nullptr;
    size_t sum = 0;
    int rc = LoadIovecs(proc, kh, iovStore, &iov, &sum);
    if (rc != 0)
        return -rc;

    size_t capacity = MIN(sum, SOCKET_IO_MAX);
    KernelBuffer data;
    if (!data.Alloc(capacity))
        return -ENOMEM;

    size_t fdCapacity = 0;
    if (kh.msgctrl != nullptr && kh.ctrllen >= CMSG_LEN(sizeof(int)))
        fdCapacity = MIN((kh.ctrllen - CMSG_LEN(0)) / sizeof(int), Net::MAX_FDS_PER_MESSAGE);

    FileDescriptor* fds[Net::MAX_FDS_PER_MESSAGE];
    Net::RecvParams params = {data.ptr, capacity, fds, fdCapacity, ref.nonBlock || (flags & MSG_DONTWAIT) != 0};
    Net::RecvResult res;
    rc = ref.sock->Recv(params, &res);
    if (rc != 0)
        return -rc;

    auto dropFDs = [&](size_t from) {
        for (size_t i = from; i < res.fdCount; i++)
            fds[i]->Unref();
    };

    size_t off = 0;
    for (size_t i = 0; i < kh.iovcount && off < res.bytes; i++) {
        size_t n = MIN(iov[i].iovLen, res.bytes - off);
        if (n > 0 && !UserWrite(iov[i].iovBase, data.As<uint8_t>() + off, n, proc)) {
            dropFDs(0);
            return -EFAULT;
        }
        off += n;
    }

    FileDescriptorManager* mgr = proc->GetFDManager();
    int newFDs[Net::MAX_FDS_PER_MESSAGE];
    size_t installed = 0;
    bool controlTruncated = res.controlTruncated;
    if (mgr != nullptr) {
        for (; installed < res.fdCount; installed++) {
            fd_t n = mgr->Allocate(fds[installed], (flags & MSG_CMSG_CLOEXEC) > 0 ? FD_CLOEXEC : 0);
            if (n < 0)
                break;
            newFDs[installed] = static_cast<int>(n);
        }
    }
    if (installed < res.fdCount) {
        dropFDs(installed);
        controlTruncated = true;
    }

    auto closeInstalled = [&]() {
        for (size_t i = 0; i < installed; i++)
            mgr->Free(newFDs[i]);
    };

    size_t ctrlUsed = 0;
    if (installed > 0) {
        size_t dataLen = installed * sizeof(int);
        KernelBuffer ctrl;
        if (!ctrl.Alloc(CMSG_SPACE(dataLen))) {
            closeInstalled();
            return -ENOMEM;
        }

        cmsghdr ch = {CMSG_LEN(dataLen), SOL_SOCKET, SCM_RIGHTS};
        memcpy(ctrl.ptr, &ch, sizeof(ch));
        memcpy(ctrl.As<uint8_t>() + CMSG_LEN(0), newFDs, dataLen);

        ctrlUsed = MIN(CMSG_SPACE(dataLen), static_cast<size_t>(kh.ctrllen));
        if (!UserWrite(kh.msgctrl, ctrl.ptr, ctrlUsed, proc)) {
            closeInstalled();
            return -EFAULT;
        }
    }

    kh.ctrllen = static_cast<socklen_t>(ctrlUsed);
    kh.addrLen = 0;
    kh.flags = (res.truncated ? MSG_TRUNC : 0) | (controlTruncated ? MSG_CTRUNC : 0);
    if (!UserWrite(hdr, &kh, sizeof(kh), proc)) {
        closeInstalled();
        return -EFAULT;
    }

    return static_cast<ssize_t>(res.bytes);
}

int sys_shutdown(int fd, int how) {
    Net::ShutdownHow h;
    switch (how) {
    case SHUT_RD:
        h = Net::ShutdownHow::Read;
        break;
    case SHUT_WR:
        h = Net::ShutdownHow::Write;
        break;
    case SHUT_RDWR:
        h = Net::ShutdownHow::Both;
        break;
    default:
        return EINVAL;
    }

    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    return ref.sock->Shutdown(h);
}

int sys_sockname(int fd, sockaddr* addr, socklen_t maxAddrLen, socklen_t* actualLen) {
    return SockNameImpl(fd, addr, maxAddrLen, actualLen, false);
}

int sys_peername(int fd, sockaddr* addr, socklen_t maxAddrLen, socklen_t* actualLen) {
    return SockNameImpl(fd, addr, maxAddrLen, actualLen, true);
}

int sys_getsockopt(int fd, int layer, int number, void* buffer, socklen_t* size) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    if (layer != SOL_SOCKET)
        return ENOPROTOOPT;
    if (buffer == nullptr || size == nullptr)
        return EFAULT;

    socklen_t userLen;
    if (!UserRead(size, &userLen, sizeof(userLen), proc))
        return EFAULT;

    uint8_t value[sizeof(Net::PeerCred)] = {};
    size_t valueLen = 0;

    switch (number) {
    case SO_TYPE: {
        int v = ref.sock->GetType() == Net::SocketType::SeqPacket ? SOCK_SEQPACKET : SOCK_STREAM;
        memcpy(value, &v, sizeof(v));
        valueLen = sizeof(v);
        break;
    }
    case SO_ERROR: {
        int v = 0;
        memcpy(value, &v, sizeof(v));
        valueLen = sizeof(v);
        break;
    }
    case SO_SNDBUF:
    case SO_RCVBUF: {
        int v = REPORTED_BUFFER_SIZE;
        memcpy(value, &v, sizeof(v));
        valueLen = sizeof(v);
        break;
    }
    case SO_PEERCRED: {
        Net::PeerCred pc;
        int rc = ref.sock->GetPeerCred(&pc);
        if (rc != 0)
            return rc;
        memcpy(value, &pc, sizeof(pc));
        valueLen = sizeof(pc);
        break;
    }
    default:
        return ENOPROTOOPT;
    }

    size_t copy = MIN(static_cast<size_t>(userLen), valueLen);
    socklen_t outLen = static_cast<socklen_t>(copy);
    if (copy > 0 && !UserWrite(buffer, value, copy, proc))
        return EFAULT;
    if (!UserWrite(size, &outLen, sizeof(outLen), proc))
        return EFAULT;
    return 0;
}

int sys_setsockopt(int fd, int layer, int number, const void* buffer, socklen_t size) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    SocketRef ref(proc, fd);
    if (ref.sock == nullptr)
        return ref.err;

    if (layer != SOL_SOCKET)
        return ENOPROTOOPT;

    switch (number) {
    case SO_REUSEADDR:
    case SO_SNDBUF:
    case SO_RCVBUF: {
        if (size < sizeof(int))
            return EINVAL;
        int v;
        if (buffer == nullptr || !UserRead(buffer, &v, sizeof(v), proc))
            return EFAULT;
        return 0; // accept and ignore for AF_UNIX
    }
    default:
        return ENOPROTOOPT;
    }
}
