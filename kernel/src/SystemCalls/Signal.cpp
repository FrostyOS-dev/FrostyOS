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

#include "Process.hpp"
#include "Signal.hpp"
#include "SystemCall.hpp"

#include <errno.h>

#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

int g_signalDefaultActions[NSIG] = {
	SIGACTION_IGN,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_CORE,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_CONT,
	SIGACTION_STOP,
	SIGACTION_STOP,
	SIGACTION_STOP,
	SIGACTION_STOP,
	SIGACTION_IGN,
	SIGACTION_CORE,
	SIGACTION_CORE,
	SIGACTION_TERM,
	SIGACTION_TERM,
	SIGACTION_IGN,
	SIGACTION_TERM, // also SIGPOLL
	SIGACTION_TERM,
	SIGACTION_CORE,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
	SIGACTION_IGN,
};

int sys_sigaction(int signal, sigaction_t* newAct, sigaction_t* oldAct) {
	if (signal >= NSIG || signal == SIGKILL || signal == SIGSTOP)
		return -EINVAL;

	Thread* current = Thread::GetCurrentThread();
    Process* currentProc = current->GetParent();

	assert(currentProc != nullptr);

	int rc = ESUCCESS;
	sigaction_t kNewAct;
	sigaction_t kOldAct;

	if (newAct != nullptr) {
		if (!UserRead(newAct, &kNewAct, sizeof(sigaction_t), currentProc))
			return -EFAULT;

		if ((kNewAct.flags & SA_RESTORER) == 0)
			return -EINVAL;
	}

	rc = currentProc->SetSignalAction(signal, newAct != nullptr ? &kNewAct : nullptr, oldAct != nullptr ? &kOldAct : nullptr);
	if (rc < 0)
		return rc;

	if (oldAct != nullptr && !UserWrite(oldAct, &kNewAct, sizeof(sigaction_t), currentProc))
		return -EFAULT;

	return ESUCCESS;
}

int sys_sigpending(sigset_t* set) {
	Thread* current = Thread::GetCurrentThread();
    Process* currentProc = current->GetParent();

	sigset_t kSet;
	current->GetPendingSignals(&kSet);

	if (!UserWrite(set, &kSet, sizeof(sigset_t), currentProc))
		return -EFAULT;

	return ESUCCESS;
}

int sys_sigprocmask(int how, sigset_t* set, sigset_t* oldSet) {
	Thread* current = Thread::GetCurrentThread();
    Process* currentProc = current->GetParent();

	sigset_t kNewSet;
	sigset_t kOldSet;

	if (set != nullptr) {
		if (how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK)
			return -EINVAL;

		if (!UserRead(&kNewSet, set, sizeof(sigset_t), currentProc))
			return -EFAULT;
	}

	current->ChangeSignalMask(how, set != nullptr ? &kNewSet : nullptr, oldSet != nullptr ? &kOldSet : nullptr);

	if (oldSet != nullptr && !UserWrite(oldSet, &kOldSet, sizeof(sigset_t), currentProc))
		return -EFAULT;

	return ESUCCESS;
}

[[noreturn]] void sys_sigreturn() {
	Thread* current = Thread::GetCurrentThread();

	int rc = current->RestoreSignalContext(&current->GetMutableRegisters(), current->GetExtraContext());

	assert(rc == 0); // something is very wrong if this happens, the function only returns 0 if the cpu states are null

	Processor::DisableInterrupts(); // must disable interrupts before calling RunThread
	Scheduler::RunThread(current, false); // Need to context switch back into the thread
}

int sys_kill(pid_t pid, int signal) {
	if (signal >= NSIG)
		return -EINVAL;

	if (pid <= 0)
		return -ENOSYS; // targetting anything other than a specific process is unsupported

	Process* target = Scheduler::GetProcess(pid);
	if (target == nullptr)
		return -ESRCH;

	return target->RaiseSignal(signal);
}
