/*
Copyright (©) 2025-2026  Frosty515

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

#ifndef _PROCESS_HPP
#define _PROCESS_HPP

#include <spinlock.h>
#include <stdint.h>

#include <DataStructures/AVLTree.hpp>
#include <DataStructures/LinkedList.hpp>

#include <Memory/VMM.hpp>

#include <SystemCalls/Signal.hpp>

#include "Event.hpp"
#include "Thread.hpp"
#include "ThreadList.hpp"

#define PROCESS_WAIT_STOPPED 1
#define PROCESS_WAIT_CONTINUED 2

enum class ProcessMode {
    KERNEL,
    USER
};

enum class ProcessState {
    ACTIVE,
    ZOMBIE,
    STOPPED
};

struct Credential {
    uid_t uid;
    uid_t euid;
    uid_t suid;
    gid_t gid;
    gid_t egid;
    gid_t sgid;
};

class FileDescriptorManager;
class FutexWaitQueue;

namespace FS {
    class VNode;
}

class TTY;

class Process {
public:
    Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice);
    ~Process();

    bool Start(bool insert = true);
    bool Create(bool initAlloc = true);
    void Delete();

    bool CreateMainThread(ThreadEntryPoint entryPoint);
    void SetMainThread(Thread* thread);
    Thread* GetMainThread() const;

    uint64_t AddThread(Thread* thread);
    Thread* GetThread(uint64_t tid) const;
    void RemoveThread(uint64_t tid, bool lock = true);
    void RemoveThread(Thread* thread);

    void SwitchToThread(Thread* thread);

    ProcessMode GetMode() const;
    uint8_t GetNice() const;

    void SetPID(int64_t pid);
    int64_t GetPID() const;

    void SetPPID(int64_t ppid);
    int64_t GetPPID() const;

    void SetVMM(VMM::VMM* vmm);
    VMM::VMM* GetVMM() const;

    const Credential& GetCred() const;
    void SetCred(const Credential& cred);

    FileDescriptorManager* GetFDManager();
    void SetFDManager(FileDescriptorManager* manager);

    FS::VNode* GetCWD();
    FS::VNode* GetCWDRef();
    void SetCWD(FS::VNode* cwd);

    bool Fork(Process* other, uint64_t newMainReturn, CPU_Registers* regs);

    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& GetFutextList();

    ProcessState GetState() const;
    void SetState(ProcessState state);

    int GetExitStatus() const;
    void SetExitStatus(int status);

    EventWaitQueue& GetChildWaitQueue();

    int64_t GetPGID() const;
    void SetPGID(int64_t pgid);
    int64_t GetSID() const;
    void SetSID(int64_t sid);
    TTY* GetControllingTTY() const;
    void SetControllingTTY(TTY* tty);
    bool HasExeced() const;
    void SetHasExeced(bool v);

    int SetSignalAction(int signal, sigaction_t* newAct, sigaction_t* oldAct);
    int RaiseSignal(int signal);

    // Following functions are intended for use by the Thread signal methods

    sigset_t& GetPendingSignals();
    sigaction_t* GetSignalAction(int signal);

    void AcquireSignalLock(); // uses a spinlock, but does NOT disable interrupts
    void ReleaseSignalLock();

    void Stop(int signal);
    bool Resume();
    void NotifyParentStateChange();
    bool TakeContinueNotify();
    int GetStopSignal() const;
    uint32_t GetWaitEvents() const;
    void ClearWaitEvents(uint32_t events);
    EventWaitQueue& GetContinueQueue();
    void MarkExiting();
    bool IsExiting() const;

private:
    void DiscardPendingSignals(const int* sigs, size_t count); // caller holds m_signalLock

    ProcessMode m_Mode;
    VMM::VMM* m_VMM;
    uint8_t m_Nice;
    int64_t m_PID;
    int64_t m_PPID;
    uint64_t m_nextTID;
    Thread* m_MainThread;
    ProcThreadList m_Threads;
    Credential m_cred;
    FileDescriptorManager* m_FDManager;
    FS::VNode* m_cwd;
    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*> m_futexList;

    ProcessState m_state;
    int m_exitStatus;
    EventWaitQueue m_childWaitQueue;
    
    sigaction_t m_sigActions[NSIG];
    sigset_t m_pendingSignals;
    spinlock_t m_signalLock;

    int m_stopSignal;
    uint32_t m_waitEvents;
    uint32_t m_continueNotify;
    bool m_exiting;
    EventWaitQueue m_continueQueue;

    int64_t m_PGID;
    int64_t m_SID;
    TTY* m_ctty;
    bool m_hasExeced;
};

extern Process* g_KProcess;

#endif /* _PROCESS_HPP */