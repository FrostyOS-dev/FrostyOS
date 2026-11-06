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

#ifndef _SYSCALL_SOCKET_HPP
#define _SYSCALL_SOCKET_HPP

#include <stdint.h>

#include "File.hpp"

#define AF_UNIX 1

#define SOCK_STREAM 1
#define SOCK_SEQPACKET 5

#define SOCK_NONBLOCK O_NONBLOCK
#define SOCK_CLOEXEC O_CLOEXEC

#define SOL_SOCKET 1

#define SCM_RIGHTS 1

#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_PEERCRED 17

#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define MSG_TRUNC 0x20
#define MSG_CTRUNC 0x8
#define MSG_CMSG_CLOEXEC 0x40000000

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define UIO_MAXIOV 1024

typedef unsigned int socklen_t;

struct iovec { // TODO: move to File.hpp once readv/writev are implemented
    void* iovBase;
    size_t iovLen;
};

struct msghdr {
    void* addr;
    socklen_t addrLen;
    void* iov;
    socklen_t iovcount;
    int padding0;
    void* msgctrl;
    socklen_t ctrllen;
    int padding1;
    int flags;
};

struct sockaddr {
    uint16_t family;
    uint8_t data[1];
};

struct cmsghdr {
    size_t cmsgLen;
    int cmsgLevel;
    int cmsgType;
};

namespace Net {
    static constexpr size_t MAX_FDS_PER_MESSAGE = 253;
}

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(cmsghdr)) + (len))
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(cmsghdr)) + CMSG_ALIGN(len))

// All functions below return a positive error unless specified otherwise

int sys_socket(int domain, int typeFlags, int protocol); // negative error, positive fd
int sys_socketpair(int domain, int typeFlags, int protocol, int fds[2]);

int sys_bind(int fd, const sockaddr* addr, socklen_t addrLen);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, sockaddr* addr, socklen_t addrLen, socklen_t* actualLen, int flags); // negative error, positive fd
int sys_connect(int fd, const sockaddr* addr, socklen_t addrLen);

ssize_t sys_sendmsg(int fd, msghdr* hdr, int flags); // negative error
ssize_t sys_recvmsg(int fd, msghdr* hdr, int flags); // negative error

int sys_shutdown(int fd, int how);

int sys_sockname(int fd, sockaddr* addr, socklen_t maxAddrLen, socklen_t* actualLen);
int sys_peername(int fd, sockaddr* addr, socklen_t maxAddrLen, socklen_t* actualLen);

int sys_getsockopt(int fd, int layer, int number, void* buffer, socklen_t* size);
int sys_setsockopt(int fd, int layer, int number, const void* buffer, socklen_t size);

#endif /* _SYSCALL_SOCKET_HPP */