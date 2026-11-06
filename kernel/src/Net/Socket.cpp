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

#include <errno.h>

namespace Net {

    Socket::Socket(SocketFamily family, SocketType type) : m_family(family), m_type(type), m_refCount(1) {

    }

    Socket::~Socket() {

    }

    void Socket::Ref() {
        __atomic_add_fetch(&m_refCount, 1, __ATOMIC_ACQ_REL);
    }

    void Socket::Unref() {
        if (__atomic_sub_fetch(&m_refCount, 1, __ATOMIC_ACQ_REL) == 0)
            delete this;
    }

    SocketFamily Socket::GetFamily() const {
        return m_family;
    }

    SocketType Socket::GetType() const {
        return m_type;
    }

    int Socket::Ioctl(uint64_t, void*, int*, Process*) {
        return ENOTTY;
    }

}
