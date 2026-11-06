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

#ifndef _NET_SOCKET_HPP
#define _NET_SOCKET_HPP

#include <stddef.h>
#include <stdint.h>

#include <Scheduling/Event.hpp>
#include <Scheduling/Process.hpp>

namespace FS {
    class VNode;
}

class FileDescriptor;

namespace Net {

    enum class SocketFamily {
        Unix,
        Inet,
        Invalid
    };

    enum class SocketType {
        Stream,
        SeqPacket,
        Datagram
    };

    enum class ShutdownHow {
        Read,
        Write,
        Both
    };

    struct PeerCred {
        int32_t pid;
        int32_t uid;
        int32_t gid;
    };

    struct CallerInfo {
        const Credential* cred;
        FS::VNode* cwd;
        uint32_t umask;
        PeerCred peer;
    };

    struct SendParams {
        const void* data;
        size_t length;
        FileDescriptor* const* fds;
        size_t fdCount;
        bool nonBlock;
        bool noSignal;
    };

    struct RecvParams {
        void* buffer;
        size_t length;
        FileDescriptor** fdsOut;
        size_t fdCapacity;
        bool nonBlock;
    };

    struct RecvResult {
        size_t bytes;
        size_t fdCount;
        bool truncated;
        bool controlTruncated;
    };

    class Socket {
    public:
        Socket(SocketFamily family, SocketType type);
        virtual ~Socket();

        void Ref();
        void Unref();

        SocketFamily GetFamily() const;
        SocketType GetType() const;

        virtual int Bind(const void* addr, size_t addrLen, const CallerInfo& caller) = 0;
        virtual int Listen(int backlog, const CallerInfo& caller) = 0;
        virtual int Accept(Socket** out, bool nonBlock) = 0;
        virtual int Connect(const void* addr, size_t addrLen, const CallerInfo& caller, bool nonBlock) = 0;

        virtual int Send(const SendParams& params, size_t* sent) = 0;
        virtual int Recv(const RecvParams& params, RecvResult* result) = 0;

        virtual int Shutdown(ShutdownHow how) = 0;
        virtual int Close() = 0;

        virtual int GetSockName(void* out, size_t cap, size_t* realLen) = 0;
        virtual int GetPeerName(void* out, size_t cap, size_t* realLen) = 0;
        virtual int GetPeerCred(PeerCred* out) = 0;

        virtual int Ioctl(uint64_t op, void* arg, int* result, Process* currentProc);

        virtual EventWaitQueue* GetPollQueue(uint32_t events) = 0;
        virtual uint32_t PollReady(uint32_t events) = 0;

    protected:
        SocketFamily m_family;
        SocketType m_type;

    private:
        uint64_t m_refCount;
    };
}

#endif /* _NET_SOCKET_HPP */