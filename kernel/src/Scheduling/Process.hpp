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

#include "Thread.hpp"
#include "ThreadList.hpp"

enum class ProcessMode {
    KERNEL,
    USER
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

class Process {
public:
    Process(ProcessMode mode, VMM::VMM* vmm, uint8_t nice);
    ~Process();

    bool Start();
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

    void SetPID(uint64_t pid);
    uint64_t GetPID() const;

    void SetPPID(uint64_t ppid);
    uint64_t GetPPID() const;

    void SetVMM(VMM::VMM* vmm);
    VMM::VMM* GetVMM() const;

    const Credential& GetCred() const;
    void SetCred(const Credential& cred);

    FileDescriptorManager* GetFDManager();
    void SetFDManager(FileDescriptorManager* manager);

    FS::VNode* GetCWD();
    void SetCWD(FS::VNode* cwd);

    bool Fork(Process* other, uint64_t newMainReturn);

    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*>& GetFutextList();

    int SetSignalAction(int signal, sigaction_t* newAct, sigaction_t* oldAct);
    int RaiseSignal(int signal);

    // Following functions are intended for use by the Thread signal methods

    sigset_t& GetPendingSignals();
    sigaction_t* GetSignalAction(int signal);

    void AcquireSignalLock(); // uses a spinlock, but does NOT disable interrupts
    void ReleaseSignalLock();

private:
    ProcessMode m_Mode;
    VMM::VMM* m_VMM;
    uint8_t m_Nice;
    uint64_t m_PID;
    uint64_t m_PPID;
    uint64_t m_nextTID;
    Thread* m_MainThread;
    ProcThreadList m_Threads;
    Credential m_cred;
    FileDescriptorManager* m_FDManager;
    FS::VNode* m_cwd;
    AVLTree::wAVLTree<uint64_t, FutexWaitQueue*> m_futexList;
    
    sigaction_t m_sigActions[NSIG];
    sigset_t m_pendingSignals;
    spinlock_t m_signalLock;
};

extern Process* g_KProcess;

#endif /* _PROCESS_HPP */