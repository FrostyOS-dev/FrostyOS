/*
Copyright (©) 2023  Frosty515

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

#ifndef _ERRNO_H
#define _ERRNO_H

#define ENUMERATE_ERRNO_CODES(E) \
    E(ESUCCESS, "Success (not an error)") \
    E(EADDRINUSE, "address in use") \
    E(EAFNOSUPPORT, "address family not supported") \
    E(EADDRNOTAVAIL, "address not available") \
    E(EISCONN, "already connected") \
    E(E2BIG, "argument list too long") \
    E(EDOM, "argument out of domain") \
    E(EFAULT, "bad address") \
    E(EBADF, "bad file descriptor") \
    E(EBADMSG, "bad message") \
    E(EPIPE, "broken pipe") \
    E(ECONNABORTED, "connection aborted") \
    E(EALREADY, "connection already in progress") \
    E(ECONNREFUSED, "connection refused") \
    E(ECONNRESET, "connection reset") \
    E(EXDEV, "cross device link") \
    E(EDESTADDRREQ, "destination address required") \
    E(EBUSY, "device or resource busy") \
    E(ENOTEMPTY, "directory not empty") \
    E(ENOEXEC, "executable format error") \
    E(EEXIST, "file exists") \
    E(EFBIG, "file too large") \
    E(ENAMETOOLONG, "filename too long") \
    E(ENOSYS, "function not supported") \
    E(EHOSTUNREACH, "host unreachable") \
    E(EIDRM, "identifier removed") \
    E(EILSEQ, "illegal byte sequence") \
    E(ENOTTY, "inappropriate io control operation") \
    E(EINTR, "interrupted") \
    E(EINVAL, "invalid argument") \
    E(ESPIPE, "invalid seek") \
    E(EIO, "io error") \
    E(EISDIR, "is a directory") \
    E(EMSGSIZE, "message size") \
    E(ENETDOWN, "network down") \
    E(ENETRESET, "network reset") \
    E(ENETUNREACH, "network unreachable") \
    E(ENOBUFS, "no buffer space") \
    E(ECHILD, "no child process") \
    E(ENOLINK, "no link") \
    E(ENOLOCK, "no lock available") \
    E(ENOMSG, "no message") \
    E(ENODATA, "no message available") \
    E(ENOPROTOOPT, "no protocol option") \
    E(ENOSPC, "no space on device") \
    E(ENOSR, "no stream resources") \
    E(ENODEV, "no such device") \
    E(ENXIO, "no such device or address") \
    E(ENOENT, "no such file or directory") \
    E(ESRCH, "no such process") \
    E(ENOTDIR, "not a directory") \
    E(ENOTSOCK, "not a socket") \
    E(ENOSTR, "not a stream") \
    E(ENOTCONN, "not connected") \
    E(ENOMEM, "not enough memory") \
    E(ENOTSUP, "not supported") \
    E(ECANCELED, "operation canceled") \
    E(EINPROGRESS, "operation in progress") \
    E(EPERM, "operation not permitted") \
    E(EOPNOTSUPP, "operation not supported") \
    E(EWOULDBLOCK, "operation would block") \
    E(EOWNERDEAD, "owner dead") \
    E(EACCES, "permission denied") \
    E(EPROTO, "protocol error") \
    E(EPROTONOSUPPORT, "protocol not supported") \
    E(EROFS, "read only file system") \
    E(EDEADLK, "resource deadlock would occur") \
    E(EAGAIN, "resource unavailable try again") \
    E(ERANGE, "result out of range") \
    E(ENOTRECOVERABLE, "state not recoverable") \
    E(ETIME, "stream timeout") \
    E(ETXTBSY, "text file busy") \
    E(ETIMEDOUT, "timed out") \
    E(EMFILE, "too many files open") \
    E(ENFILE, "too many files open in system") \
    E(EMLINK, "too many links") \
    E(ELOOP, "too many symbolic link levels") \
    E(EOVERFLOW, "value too large") \
    E(EPROTOTYPE, "wrong protocol type")

enum ErrorCodes {
#define __ENUMERATE_CODE(c, s) c,
    ENUMERATE_ERRNO_CODES(__ENUMERATE_CODE)
#undef __ENUMERATE_CODE
};

#define ESUCCESS ESUCCESS
#define EADDRINUSE EADDRINUSE
#define EAFNOSUPPORT EAFNOSUPPORT
#define EISCONN EISCONN
#define EADDRNOTAVAIL EADDRNOTAVAIL
#define EDOM EDOM
#define E2BIG E2BIG
#define EBADF EBADF
#define EFAULT EFAULT
#define EPIPE EPIPE
#define EBADMSG EBADMSG
#define EALREADY EALREADY
#define ECONNABORTED ECONNABORTED
#define ECONNRESET ECONNRESET
#define ECONNREFUSED ECONNREFUSED
#define EDESTADDRREQ EDESTADDRREQ
#define EXDEV EXDEV
#define ENOTEMPTY ENOTEMPTY
#define EBUSY EBUSY
#define EEXIST EEXIST
#define ENOEXEC ENOEXEC
#define ENAMETOOLONG ENAMETOOLONG
#define EFBIG EFBIG
#define EHOSTUNREACH EHOSTUNREACH
#define ENOSYS ENOSYS
#define EILSEQ EILSEQ
#define EIDRM EIDRM
#define EINTR EINTR
#define ENOTTY ENOTTY
#define ESPIPE ESPIPE
#define EINVAL EINVAL
#define EISDIR EISDIR
#define EIO EIO
#define ENETDOWN ENETDOWN
#define EMSGSIZE EMSGSIZE
#define ENETUNREACH ENETUNREACH
#define ENETRESET ENETRESET
#define ECHILD ECHILD
#define ENOBUFS ENOBUFS
#define ENOLOCK ENOLOCK
#define ENOLINK ENOLINK
#define ENODATA ENODATA
#define ENOMSG ENOMSG
#define ENOSPC ENOSPC
#define ENOPROTOOPT ENOPROTOOPT
#define ENODEV ENODEV
#define ENOSR ENOSR
#define ENOENT ENOENT
#define ENXIO ENXIO
#define ENOTDIR ENOTDIR
#define ESRCH ESRCH
#define ENOSTR ENOSTR
#define ENOTSOCK ENOTSOCK
#define ENOMEM ENOMEM
#define ENOTCONN ENOTCONN
#define ECANCELED ECANCELED
#define ENOTSUP ENOTSUP
#define EPERM EPERM
#define EINPROGRESS EINPROGRESS
#define EWOULDBLOCK EWOULDBLOCK
#define EOPNOTSUPP EOPNOTSUPP
#define EACCES EACCES
#define EOWNERDEAD EOWNERDEAD
#define EPROTONOSUPPORT EPROTONOSUPPORT
#define EPROTO EPROTO
#define EDEADLK EDEADLK
#define EROFS EROFS
#define ERANGE ERANGE
#define EAGAIN EAGAIN
#define ETIME ETIME
#define ENOTRECOVERABLE ENOTRECOVERABLE
#define ETIMEDOUT ETIMEDOUT
#define ETXTBSY ETXTBSY
#define ENFILE ENFILE
#define EMFILE EMFILE
#define ELOOP ELOOP
#define EMLINK EMLINK
#define EPROTOTYPE EPROTOTYPE
#define EOVERFLOW EOVERFLOW

#endif /* _ERRNO_H */