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

#ifndef _NET_UNIX_SOCKET_HPP
#define _NET_UNIX_SOCKET_HPP

#include <stddef.h>
#include <stdint.h>

#include <Scheduling/Event.hpp>
#include <Scheduling/Mutex.hpp>

#include "Socket.hpp"

namespace Net {

    struct UnixConnection;

    constexpr uint16_t UNIX_ADDRESS_FAMILY = 1;
    constexpr size_t UNIX_PATH_MAX = 108;

    struct SockAddrUn {
        uint16_t family;
        char path[UNIX_PATH_MAX];
    };

    class UnixSocket final : public Socket {
    public:
        explicit UnixSocket(SocketType type);
        ~UnixSocket() override;

        static int CreatePair(SocketType type, const PeerCred& creator, UnixSocket** a, UnixSocket** b);

        int Bind(const void* addr, size_t addrLen, const CallerInfo& caller) override;
        int Listen(int backlog, const CallerInfo& caller) override;
        int Accept(Socket** out, bool nonBlock) override;
        int Connect(const void* addr, size_t addrLen, const CallerInfo& caller, bool nonBlock) override;

        int Send(const SendParams& params, size_t* sent) override;
        int Recv(const RecvParams& params, RecvResult* result) override;

        int Shutdown(ShutdownHow how) override;
        int Close() override;

        int GetSockName(void* out, size_t cap, size_t* realLen) override;
        int GetPeerName(void* out, size_t cap, size_t* realLen) override;
        int GetPeerCred(PeerCred* out) override;

        int Ioctl(uint64_t op, void* arg, int* result, Process* currentProc) override;

        EventWaitQueue* GetPollQueue(uint32_t events) override;
        uint32_t PollReady(uint32_t events) override;

    private:
        enum class State {
            Unbound,
            Binding,
            Bound,
            Listening,
            Connecting,
            Connected,
            Closed
        };

        int GetConnection(UnixConnection** conn, int* side);
        int EnqueueIncoming(UnixConnection* conn, bool nonBlock);

        Mutex m_lock;
        State m_state;

        EventWaitQueue m_listenQueue;

        UnixConnection* m_conn;
        int m_side;

        FS::VNode* m_vnode;
        char m_path[UNIX_PATH_MAX + 1];
        size_t m_pathLen;

        UnixConnection* m_backlogHead;
        UnixConnection* m_backlogTail;
        size_t m_backlogCount;
        size_t m_backlogMax;
        PeerCred m_listenerCred;
    };
}

#endif /* _NET_UNIX_SOCKET_HPP */