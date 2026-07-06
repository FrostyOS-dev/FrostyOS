/*
Copyright (©) 2023-2026  Frosty515

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
    E(ESUCCESS, "Success") \
    E(EAGAIN, "Operation would block (EAGAIN)") \
    E(EACCES, "Access denied (EACCESS)") \
    E(EBADF, "Bad file descriptor (EBADF)") \
    E(EEXIST, "File exists already (EEXIST)") \
    E(EFAULT, "Access violation (EFAULT)") \
    E(EINTR, "Operation interrupted (EINTR)") \
    E(EINVAL, "Invalid argument (EINVAL)") \
    E(EIO, "I/O error (EIO)") \
    E(EISDIR, "Resource is directory (EISDIR)") \
    E(ENOENT, "No such file or directory (ENOENT)") \
    E(ENOMEM, "Out of memory (ENOMEM)") \
    E(ENOTDIR, "Expected directory instead of file (ENOTDIR)") \
    E(ENOSYS, "Operation not implemented (ENOSYS)") \
    E(EPERM, "Operation not permitted (EPERM)") \
    E(EPIPE, "Broken pipe (EPIPE)") \
    E(ESPIPE, "Seek not possible (ESPIPE)") \
    E(ENXIO, "No such device or address (ENXIO)") \
    E(ENOEXEC, "Exec format error (ENOEXEC)") \
    E(ENOSPC, "No space left on device (ENOSPC)") \
    E(ENOTSOCK, "Socket operation on non-socket (ENOTSOCK)") \
    E(ENOTCONN, "Transport endpoint is not connected (ENOTCONN)") \
    E(EDOM, "Numerical argument out of domain (EDOM)") \
    E(EILSEQ, "Invalid or incomplete multibyte or wide character (EILSEQ)") \
    E(ERANGE, "Numerical result out of range (ERANGE)") \
    E(E2BIG, "Argument list too long (E2BIG)") \
    E(EADDRINUSE, "Address already in use (EADDRINUSE)") \
    E(EADDRNOTAVAIL, "Cannot assign requested address (EADDRNOTAVAIL)") \
    E(EAFNOSUPPORT, "Address family not supported by protocol (EAFNOSUPPORT)") \
    E(EALREADY, "Operation already in progress (EALREADY)") \
    E(EBADMSG, "Bad message (EBADMSG)") \
    E(EBUSY, "Device or resource busy (EBUSY)") \
    E(ECANCELED, "Operation canceled (ECANCELED)") \
    E(ECHILD, "No child processes (ECHILD)") \
    E(ECONNABORTED, "Software caused connection abort (ECONNABORTED)") \
    E(ECONNREFUSED, "Connection refused (ECONNREFUSED)") \
    E(ECONNRESET, "Connection reset by peer (ECONNRESET)") \
    E(EDEADLK, "Resource deadlock avoided (EDEADLK)") \
    E(EDESTADDRREQ, "Destination address required (EDESTADDRREQ)") \
    E(EDQUOT, "Disk quota exceeded (EDQUOT)") \
    E(EFBIG, "File too large (EFBIG)") \
    E(EHOSTUNREACH, "No route to host (EHOSTUNREACH)") \
    E(EIDRM, "Identifier removed (EIDRM)") \
    E(EINPROGRESS, "Operation now in progress (EINPROGRESS)") \
    E(EISCONN, "Transport endpoint is already connected (EISCONN)") \
    E(ELOOP, "Too many levels of symbolic links (ELOOP)") \
    E(EMFILE, "Too many open files (EMFILE)") \
    E(EMLINK, "Too many links (EMLINK)") \
    E(EMSGSIZE, "Message too long (EMSGSIZE)") \
    E(EMULTIHOP, "Multihop attempted (EMULTIHOP)") \
    E(ENAMETOOLONG, "File name too long (ENAMETOOLONG)") \
    E(ENETDOWN, "Network is down (ENETDOWN)") \
    E(ENETRESET, "Network dropped connection on reset (ENETRESET)") \
    E(ENETUNREACH, "Network is unreachable (ENETUNREACH)") \
    E(ENFILE, "Too many open files in system (ENFILE)") \
    E(ENOBUFS, "No buffer space available (ENOBUFS)") \
    E(ENODEV, "No such device (ENODEV)") \
    E(ENOLCK, "No locks available (ENOLCK)") \
    E(ENOLINK, "Link has been severed (ENOLINK)") \
    E(ENOMSG, "No message of desired type (ENOMSG)") \
    E(ENOPROTOOPT, "Protocol not available (ENOPROTOOPT)") \
    E(ENOTEMPTY, "Directory not empty (ENOTEMPTY)") \
    E(ENOTRECOVERABLE, "Sate not recoverable (ENOTRECOVERABLE)") \
    E(ENOTSUP, "Operation not supported (ENOTSUP)") \
    E(ENOTTY, "Inappropriate ioctl for device (ENOTTY)") \
    E(EOVERFLOW, "Value too large for defined datatype (EOVERFLOW)") \
    E(EOWNERDEAD, "Owner died (EOWNERDEAD)") \
    E(EPROTO, "Protocol error (EPROTO)") \
    E(EPROTONOSUPPORT, "Protocol not supported (EPROTONOSUPPORT)") \
    E(EPROTOTYPE, "Protocol wrong type for socket (EPROTOTYPE)") \
    E(EROFS, "Read-only file system (EROFS)") \
    E(ESRCH, "No such process (ESRCH)") \
    E(ESTALE, "Stale file handle (ESTALE)") \
    E(ETIMEDOUT, "Connection timed out (ETIMEDOUT)") \
    E(ETXTBSY, "Text file busy (ETXTBSY)") \
    E(EXDEV, "Invalid cross-device link (EXDEV)") \
    E(ENODATA, "No data available (ENODATA)") \
    E(ETIME, "Timer expired (ETIME)") \
    E(ENOKEY, "Required key not available (ENOKEY)") \
    E(ESHUTDOWN, "Cannot send after transport endpoint shutdown (ESHUTDOWN)") \
    E(EHOSTDOWN, "Host is down (EHOSTDOWN)") \
    E(EBADFD, "File descriptor in bad state (EBADFD)") \
    E(ENOMEDIUM, "No medium found (ENOMEDIUM)") \
    E(ENOTBLK, "Block device required (ENOTBLK)") \
    E(ENONET, "Machine is not on the network (ENONET)") \
    E(EPFNOSUPPORT, "Protocol family not supported (EPFNOSUPPORT)") \
    E(ESOCKTNOSUPPORT, "Socket type not supported (ESOCKTNOSUPPORT)") \
    E(ESTRPIPE, "Streams pipe error (ESTRPIPE)") \
    E(EREMOTEIO, "Remote I/O error (EREMOTEIO)") \
    E(ERFKILL, "Operation not possible due to RF-kill (ERFKILL)") \
    E(EBADR, "Invalid request descriptor (EBADR)") \
    E(EUNATCH, "Protocol driver not attached (EUNATCH)") \
    E(EMEDIUMTYPE, "Wrong medium type (EMEDIUMTYPE)") \
    E(EREMOTE, "Object is remote (EREMOTE)") \
    E(EKEYREJECTED, "Key was rejected by service (EKEYREJECTED)") \
    E(EUCLEAN, "Structure needs cleaning (EUCLEAN)") \
    E(EBADSLT, "Invalid slot (EBADSLT)") \
    E(ENOANO, "No anode (ENOANO)") \
    E(ENOCSI, "No CSI structure available (ENOCSI)") \
    E(ENOSTR, "Device not a stream (ENOSTR)") \
    E(ETOOMANYREFS, "Too many references: cannot splice (ETOOMANYREFS)") \
    E(ENOPKG, "Package not installed (ENOPKG)") \
    E(EKEYREVOKED, "Key has been revoked (EKEYREVOKED)") \
    E(EXFULL, "Exchange full (EXFULL)") \
    E(ELNRNG, "Link number out of range (ELNRNG)") \
    E(ENOTUNIQ, "Name not unique on network (ENOTUNIQ)") \
    E(ERESTART, "Interrupted system call should be restarted (ERESTART)") \
    E(EUSERS, "Too many users (EUSERS)")

enum ErrorCodes {
#define __ENUMERATE_CODE(c, s) c,
    ENUMERATE_ERRNO_CODES(__ENUMERATE_CODE)
#undef __ENUMERATE_CODE
};

#define ESUCCESS         0
#define EPERM            1
#define ENOENT           2
#define ESRCH            3
#define EINTR            4
#define EIO              5
#define ENXIO            6
#define E2BIG            7
#define ENOEXEC          8
#define EBADF            9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define ENOTBLK         15
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define ETXTBSY         26
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         35
#define ENAMETOOLONG    36
#define ENOLCK          37
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define EWOULDBLOCK     EAGAIN
#define ENOMSG          42
#define EIDRM           43
#define ECHRNG          44
#define EL2NSYNC        45
#define EL3HLT          46
#define EL3RST          47
#define ELNRNG          48
#define EUNATCH         49
#define ENOCSI          50
#define EL2HLT          51
#define EBADE           52
#define EBADR           53
#define EXFULL          54
#define ENOANO          55
#define EBADRQC         56
#define EBADSLT         57
#define EDEADLOCK       EDEADLK
#define EBFONT          59
#define ENOSTR          60
#define ENODATA         61
#define ETIME           62
#define ENOSR           63
#define ENONET          64
#define ENOPKG          65
#define EREMOTE         66
#define ENOLINK         67
#define EADV            68
#define ESRMNT          69
#define ECOMM           70
#define EPROTO          71
#define EMULTIHOP       72
#define EDOTDOT         73
#define EBADMSG         74
#define EOVERFLOW       75
#define ENOTUNIQ        76
#define EBADFD          77
#define EREMCHG         78
#define ELIBACC         79
#define ELIBBAD         80
#define ELIBSCN         81
#define ELIBMAX         82
#define ELIBEXEC        83
#define EILSEQ          84
#define ERESTART        85
#define ESTRPIPE        86
#define EUSERS          87
#define ENOTSOCK        88
#define EDESTADDRREQ    89
#define EMSGSIZE        90
#define EPROTOTYPE      91
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define ESOCKTNOSUPPORT 94
#define EOPNOTSUPP      95
#define ENOTSUP         EOPNOTSUPP
#define EPFNOSUPPORT    96
#define EAFNOSUPPORT    97
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99
#define ENETDOWN        100
#define ENETUNREACH     101
#define ENETRESET       102
#define ECONNABORTED    103
#define ECONNRESET      104
#define ENOBUFS         105
#define EISCONN         106
#define ENOTCONN        107
#define ESHUTDOWN       108
#define ETOOMANYREFS    109
#define ETIMEDOUT       110
#define ECONNREFUSED    111
#define EHOSTDOWN       112
#define EHOSTUNREACH    113
#define EALREADY        114
#define EINPROGRESS     115
#define ESTALE          116
#define EUCLEAN         117
#define ENOTNAM         118
#define ENAVAIL         119
#define EISNAM          120
#define EREMOTEIO       121
#define EDQUOT          122
#define ENOMEDIUM       123
#define EMEDIUMTYPE     124
#define ECANCELED       125
#define ENOKEY          126
#define EKEYEXPIRED     127
#define EKEYREVOKED     128
#define EKEYREJECTED    129
#define EOWNERDEAD      130
#define ENOTRECOVERABLE 131
#define ERFKILL         132
#define EHWPOISON       133

extern int errno;

#endif /* _ERRNO_H */