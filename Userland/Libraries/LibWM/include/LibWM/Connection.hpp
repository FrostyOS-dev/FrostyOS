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

#ifndef _FOS_LIBWM_CONNECTION_HPP
#define _FOS_LIBWM_CONNECTION_HPP

#include <deque>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>
#include <vector>

#include <sys/types.h>

#include "Message.hpp"

namespace LibWM {
    enum class Status {
        Ok,
        WouldBlock,
        Closed,
        Error,
        Protocol
    };

    struct Message {
        MessageHeader header;
        const uint8_t* payload;
        size_t payloadSize;
    };

    template <typename T>
    bool Parse(const Message& m, T& out) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (m.payloadSize != sizeof(T))
            return false;
        memcpy(&out, m.payload, sizeof(T));
        return true;
    }

    class Connection {
    public:
        Connection() = default;
        explicit Connection(int fd) : m_fd(fd) {}
        ~Connection() { Close(); }

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&& o) noexcept { Take(o); }
        Connection& operator=(Connection&& o) noexcept {
            if (this != &o) {
                Close();
                Take(o);
            }
            return *this;
        }

        static Connection Connect(const char* path);

        bool Valid() const { return m_fd >= 0; }
        int FD() const { return m_fd; }
        void Close();

        bool SetNonBlocking(bool on);

        bool PeerCred(pid_t& pid, uid_t& uid, gid_t& gid) const;

        bool Queue(uint32_t object, uint16_t opcode, const void* payload, size_t len, const int* fds = nullptr, size_t nfds = 0);

        template <typename T>
        bool Send(uint32_t object, uint16_t opcode, const T& body, const int* fds = nullptr, size_t nfds = 0) {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T) % 4 == 0, "Message bodies must be 4-byte multiples");
            return Queue(object, opcode, &body, sizeof(T), fds, nfds);
        }
        bool Send(uint32_t object, uint16_t opcode) {
            return Queue(object, opcode, nullptr, 0);
        }

        Status Flush();
        bool WantsWrite() const {
            return !m_out.empty();
        }

        Status Fill();

        bool Next(Message& out);
        bool ProtocolError() const {
            return m_protoError;
        }

        int TakeFD();

    private:
        struct Chunk {
            std::vector<uint8_t> bytes;
            std::vector<int> fds;
            size_t sent = 0;
        };

        void Take(Connection& o);

        int m_fd = -1;
        std::deque<Chunk> m_out;
        size_t m_outBytes = 0;

        std::vector<uint8_t> m_in = std::vector<uint8_t>(2 * MaxMessageSize);
        size_t m_inPos = 0;
        size_t m_inEnd = 0;
        std::deque<int> m_inFDs;
        bool m_protoError = false;
    };

    class Listener {
    public:
        Listener() = default;
        ~Listener();
        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;

        bool Listen(const char* path, mode_t mode = 0666);
        int FD() const { return m_fd; }

        Connection Accept();

    private:
        int m_fd = -1;
        char m_path[108] = {};
    };
}

#endif /* _FOS_LIBWM_CONNECTION_HPP */