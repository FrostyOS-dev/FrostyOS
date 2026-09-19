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

#ifndef _SYSCALL_SIGNAL_HPP
#define _SYSCALL_SIGNAL_HPP

#include <stdint.h>

#include "Process.hpp"

typedef struct {
	uint64_t sig[1024 / 64];
} sigset_t;

#define SIGNAL_GET(set, i) ((set)->sig[(i - 1) / 64] & (1UL << (((i) - 1) % 64)))
#define SIGNAL_SET(set, i) (set)->sig[(i - 1) / 64] |= (1UL << (((i) - 1) % 64))
#define SIGNAL_CLEAR(set, i) (set)->sig[(i - 1) / 64] &= ~(1UL << (((i) - 1) % 64))

#define SIG_DFL ((void*)0)
#define SIG_IGN ((void*)1)

#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO 4
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000
#define SA_RESTORER 0x04000000

#define NSIG 65

struct sigaction_t {
    void* address;
    int flags;
    void (*restorer)(void);
    sigset_t mask;
};

union sigval {
	int sival_int;
	void *sival_ptr;
};

typedef long clock_t;

// struct taken from musl.

typedef struct {
	int si_signo, si_errno, si_code;
	union {
		char __pad[128 - 2*sizeof(int) - sizeof(long)];
		struct {
			union {
				struct {
					pid_t si_pid;
					uid_t si_uid;
				} __piduid;
				struct {
					int si_timerid;
					int si_overrun;
				} __timer;
			} __first;
			union {
				union sigval si_value;
				struct {
					int si_status;
					clock_t si_utime, si_stime;
				} __sigchld;
			} __second;
		} __si_common;
		struct {
			void *si_addr;
			short si_addr_lsb;
			union {
				struct {
					void *si_lower;
					void *si_upper;
				} __addr_bnd;
				unsigned si_pkey;
			} __first;
		} __sigfault;
		struct {
			long si_band;
			int si_fd;
		} __sigpoll;
		struct {
			void *si_call_addr;
			int si_syscall;
			unsigned si_arch;
		} __sigsys;
	} __si_fields;
} siginfo_t;

#define SIGHUP    1
#define SIGQUIT   3
#define SIGTRAP   5
#define SIGABRT 6
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGWINCH  28
#define SIGPOLL   29
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define SIGCANCEL 32
#define SIGABRT 6
#define SIGFPE 8
#define SIGILL 4
#define SIGINT 2
#define SIGSEGV 11
#define SIGTERM 15
#define SIGPROF 27
#define SIGIO 29
#define SIGPWR 30
#define SIGRTMIN 35
#define SIGRTMAX 64

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGACTION_TERM 0
#define SIGACTION_IGN 1
#define SIGACTION_CORE 2
#define SIGACTION_STOP 3
#define SIGACTION_CONT 4

extern int g_signalDefaultActions[NSIG];

int sys_sigaction(int signal, sigaction_t* newAct, sigaction_t* oldAct);
int sys_sigpending(sigset_t* set);
int sys_sigprocmask(int how, sigset_t* set, sigset_t* oldSet);
[[noreturn]] void sys_sigreturn();
int sys_kill(pid_t pid, int signal);

#endif /* _SYSCALL_SIGNAL_HPP */