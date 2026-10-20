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
#include "SystemCall.hpp"

#include <errno.h>
#include <stdint.h>

#include <Exec/ELF.hpp>

#include <fs/FDManager.hpp>
#include <fs/VFS.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>
#include <Memory/VMRegionAllocator.hpp>

#include <Scheduling/Event.hpp>
#include <Scheduling/Process.hpp>
#include <Scheduling/Scheduler.hpp>
#include <Scheduling/Thread.hpp>

#ifdef __x86_64__
#include <arch/x86_64/Scheduling/TaskUtil.hpp>
#endif

#define WNOHANG 1
#define WUNTRACED 2
#define WCONTINUED 8

[[noreturn]] void sys_exit(uint64_t code) {
    Thread::ExitCurrentThread((code & 0xFF) << 8, true, true, true);

    PANIC("sys_exit failed!");
}

int sys_settcb(void* base) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return -ENOSYS;

    if (!vmm->ValidateRead(base, 1))
        return -EFAULT;

#ifdef __x86_64__
    current->GetExtraContext()->fsBase = (uint64_t)base;
    x86_64_SetFSBase((uint64_t)base);
#endif

    return ESUCCESS;
}

pid_t sys_getpid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPID();
}

pid_t sys_getppid() {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPPID();
}

pid_t sys_gettid() {
    return Thread::GetCurrentThread()->GetTID();
}

int sys_getresuid(UIDs* uids) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();

    const Credential& cred = proc->GetCred();
    UIDs kUIDS = {cred.uid, cred.euid, cred.suid};

    if (!UserWrite(uids, &kUIDS, sizeof(UIDs), proc))
        return -EFAULT;

    return ESUCCESS;
}

int sys_getresgid(GIDs* gids) {
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();

    const Credential& cred = proc->GetCred();
    GIDs kGIDS = {cred.gid, cred.egid, cred.sgid};

    if (!UserWrite(gids, &kGIDS, sizeof(GIDs), proc))
        return -EFAULT;

    return ESUCCESS;
}

static int SetRes(int* r, int* e, int* s, int nr, int ne, int ns, bool privileged) {
    if (nr < -1 || ne < -1 || ns < -1)
        return -EINVAL;
    auto allowed = [&](int v) { return v == -1 || v == *r || v == *e || v == *s; };
    if (!privileged && (!allowed(nr) || !allowed(ne) || !allowed(ns)))
        return -EPERM;
    if (nr != -1) *r = nr;
    if (ne != -1) *e = ne;
    if (ns != -1) *s = ns;
    return ESUCCESS;
}

int sys_setresuid(UIDs* uids) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    UIDs kUIDs{};
    if (!UserRead(uids, &kUIDs, sizeof(UIDs), proc))
        return -EFAULT;

    Credential c = proc->GetCred();
    int rc = SetRes(&c.uid, &c.euid, &c.suid, kUIDs.ruid, kUIDs.euid, kUIDs.suid, c.euid == 0);
    if (rc == ESUCCESS)
        proc->SetCred(c);
    return rc;
}

int sys_setresgid(GIDs* gids) {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    GIDs kGIDs{};
    if (!UserRead(gids, &kGIDs, sizeof(GIDs), proc))
        return -EFAULT;

    Credential c = proc->GetCred();
    int rc = SetRes(&c.gid, &c.egid, &c.sgid, kGIDs.rgid, kGIDs.egid, kGIDs.sgid, c.euid == 0);
    if (rc == ESUCCESS)
        proc->SetCred(c);
    return rc;
}

pid_t sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, CPU_Registers* state) {
    Thread* current = Thread::GetCurrentThread();
    Process* currentProc = current->GetParent();
    VMM::VMM* currentVMM = currentProc->GetVMM();

    Process* proc = new Process(ProcessMode::USER, nullptr, 15);
    FileDescriptorManager* FDManager = new FileDescriptorManager(); // need to create here so we can fork it
    if (proc == nullptr || FDManager == nullptr) {
        if (proc != nullptr)
            delete proc;
        if (FDManager != nullptr)
            delete FDManager;
        return -ENOMEM;
    }

    if (!FDManager->Fork(currentProc->GetFDManager())) {
        delete FDManager;
        delete proc;
        return -ENOMEM;
    }

    proc->SetFDManager(FDManager);
    proc->SetCred(currentProc->GetCred());
    proc->SetCWD(currentProc->GetCWDRef());
    proc->SetPGID(currentProc->GetPGID());
    proc->SetSID(currentProc->GetSID());
    proc->SetControllingTTY(currentProc->GetControllingTTY());

    if (!proc->Create(false)) {
        FDManager->Delete();
        delete FDManager;
        delete proc;
        return -ENOMEM;
    }

    proc->SetPPID(currentProc->GetPID());

    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr || vmm->GetAllocator() == nullptr || currentVMM == nullptr || currentVMM->GetAllocator() == nullptr) {
        proc->Delete();
        delete proc;
        return -ENOSYS;
    }

    VMRegionAllocator* allocator = vmm->GetAllocator();

    if (!vmm->Fork(currentVMM) || !allocator->Fork(currentVMM->GetAllocator()) || !proc->Fork(currentProc, 0, state)) {
        proc->Delete();
        delete proc;
        return -ENOMEM;
    }

    return proc->GetPID();
}

void CleanupArgEnv(uint64_t argc, uint64_t argIndex, char** argv, uint64_t envc, uint64_t envIndex, char** env) {
    if (argv != nullptr) {
        for (uint64_t i = argIndex; i > 0; i--) { // work backwards as at and after the current index is still the user pointer
            if (argv[i - 1] != nullptr)
                kfree(argv[i - 1]);
        }
        kfree(argv);
    }

    if (env != nullptr) {
        for (uint64_t i = envIndex; i > 0; i--) { // work backwards as at and after the current index is still the user pointer
            if (env[i - 1] != nullptr)
                kfree(env[i - 1]);
        }
        kfree(env);
    }
}

int CopyArgEnvFromUser(const char** argv, const char** envv, uint64_t* kArgc, char*** kArgv, uint64_t* kEnvc, char*** kEnv, Process* currentProc) {
    char** currentArgv = (char**)kmalloc(8 * sizeof(char*));
    uint64_t argc = 0;
    uint64_t maxArgc = 8;
    while (true) {
        char* arg;
        if (!UserRead(&argv[argc], &arg, sizeof(char*), currentProc)) {
            kfree(currentArgv);
            return -EFAULT;
        }
        if (argc == maxArgc) {
            maxArgc += 8;
            void* oldArgv = currentArgv;
            currentArgv = (char**)krealloc(currentArgv, maxArgc * sizeof(char*));
            if (currentArgv == nullptr) {
                kfree(oldArgv);
                return -ENOMEM;
            }
        }
        currentArgv[argc] = arg;
        if (arg == nullptr)
            break;
        argc++;
    }

    char** currentEnv = (char**)kmalloc(8 * sizeof(char*));
    uint64_t envc = 0;
    uint64_t maxEnvc = 8;
    while (true) {
        char* env;
        if (!UserRead(&envv[envc], &env, sizeof(char*), currentProc)) {
            kfree(currentArgv);
            kfree(currentEnv);
            return -EFAULT;
        }
        if (envc == maxEnvc) {
            maxEnvc += 8;
            void* oldEnv = currentEnv;
            currentEnv = (char**)krealloc(currentEnv, maxEnvc * sizeof(char*));
            if (currentEnv == nullptr) {
                kfree(currentArgv);
                kfree(oldEnv);
                return -ENOMEM;
            }
        }
        currentEnv[envc] = env;
        if (env == nullptr)
            break;
        envc++;
    }

    for (uint64_t i = 0; i < argc; i++) {
        char* arg;
        size_t size;
        if (!UserReadString(currentArgv[i], &arg, &size, currentProc)) {
            CleanupArgEnv(argc, i, currentArgv, envc, 0, currentEnv);
            return -EFAULT;
        }
        currentArgv[i] = arg;
    }

    for (uint64_t i = 0; i < envc; i++) {
        char* env;
        size_t size;
        if (!UserReadString(currentEnv[i], &env, &size, currentProc)) {
            CleanupArgEnv(argc, argc, currentArgv, envc, i, currentEnv);
            return -EFAULT;
        }
        currentEnv[i] = env;
    }

    *kArgc = argc;
    *kArgv = currentArgv;
    *kEnvc = envc;
    *kEnv = currentEnv;
    return ESUCCESS;
}

int sys_exec(const char* path, char* const argv[], char* const env[]) {
    if (path == nullptr || argv == nullptr || env == nullptr)
        return -EFAULT;

    Thread* current = Thread::GetCurrentThread();
    Process* currentProc = current->GetParent();

    Process* newProc = nullptr;

    Process* parent = nullptr;
    pid_t parentPID = currentProc->GetPPID();
    if (parentPID >= 0) {
        parent = Scheduler::GetProcess(parentPID);
        if (parent == nullptr) { // process must have died, try again with PID 1
            parentPID = 1;
            parent = Scheduler::GetProcess(parentPID);
            if (parent == nullptr) // no process with PID 1???
                return -ENOSYS;
        }
    }

    size_t pathLen = 0;
    char* kPath = nullptr;
    if (!UserReadString(path, &kPath, &pathLen, currentProc))
        return -EFAULT;

    uint64_t argc;
    uint64_t envc;
    char** kArgv;
    char** kEnv;
    int rc = CopyArgEnvFromUser(const_cast<const char**>(argv), const_cast<const char**>(env), &argc, &kArgv, &envc, &kEnv, currentProc);
    if (rc < 0) {
        delete kPath;
        return rc;
    }

    Credential callerCred = currentProc->GetCred();
    rc = CreateELFProcess(kPath, parent, kArgv, kEnv, true, &newProc, &callerCred, currentProc->GetCWD());

    delete kPath;
    CleanupArgEnv(argc, argc, kArgv, envc, envc, kEnv);

    if (rc < 0)
        return rc;

    newProc->SetPID(currentProc->GetPID());
    newProc->SetPPID(parentPID);
    newProc->SetPGID(currentProc->GetPGID());
    newProc->SetSID(currentProc->GetSID());
    newProc->SetControllingTTY(currentProc->GetControllingTTY());
    newProc->SetHasExeced(true);
    for (int s = 1; s < NSIG; s++) { // SIG_IGN survives exec, handlers reset
        sigaction_t* old = currentProc->GetSignalAction(s);
        if (old->address == SIG_IGN) {
            sigaction_t act = {};
            act.address = SIG_IGN;
            newProc->SetSignalAction(s, &act, nullptr);
        }
    }

    FS::UnrefVNode(newProc->GetCWD());
    newProc->SetCWD(currentProc->GetCWDRef());

    FileDescriptorManager* FDManager = newProc->GetFDManager();
    if (!FDManager->Exec(currentProc->GetFDManager())) {
        newProc->Delete();
        delete newProc;
        return -ENOMEM;
    }

    Scheduler::g_Processes.lock();
    Scheduler::g_Processes.remove(currentProc);
    Scheduler::g_Processes.unlock();

    if (!newProc->Start()) {
        newProc->Delete();
        delete newProc;
        Scheduler::g_Processes.lock();
        Scheduler::g_Processes.insert(currentProc);
        Scheduler::g_Processes.unlock();
        return -ENOTSUP; // very unlikely to happen
    }

    currentProc->GetFDManager()->Delete(); // clear the descriptor table to ensure FDs close immediately

    Thread::ExitCurrentThread(0, true, true, false);

    PANIC("sys_exec: Thread::ExitCurrentThread returned!");
}

long sys_waitpid(pid_t pid, int* wstatus, int options) {
    Thread* currentThread = Thread::GetCurrentThread();
    Process* currentProcess = currentThread->GetParent();
    
    while (true) {
        EventWaitNode node = {};
        node.queue = &currentProcess->GetChildWaitQueue();
        node.requestedEvents = PROCESS_EXIT_EVENT;
        EventWaitNode* nodePtr = &node;
        Event::RegisterWaitNodes(&nodePtr, 1);

        bool hasChildren = false;
        Process* hit = nullptr;
        uint32_t hitEvent = 0;
        int64_t myPID = currentProcess->GetPID();
        int64_t myPGID = currentProcess->GetPGID();
        
        Scheduler::g_Processes.lock();
        Scheduler::g_Processes.Enumerate([&](Process* p, void* data) -> bool {
            if (p->GetPPID() != myPID)
                return true;
            bool match = (pid == -1) || (pid > 0 && p->GetPID() == pid) || (pid == 0 && p->GetPGID() == myPGID) || (pid < -1 && p->GetPGID() == -pid);
            if (!match)
                return true;
            hasChildren = true;
            if (p->GetState() == ProcessState::ZOMBIE) {
                hit = p;
                return false;
            }
            uint32_t ev = p->GetWaitEvents();
            if ((options & WUNTRACED) > 0 && (ev & PROCESS_WAIT_STOPPED) > 0 && p->GetState() == ProcessState::STOPPED) {
                hit = p;
                hitEvent = PROCESS_WAIT_STOPPED;
                return false;
            }
            if ((options & WCONTINUED) > 0 && (ev & PROCESS_WAIT_CONTINUED) > 0) {
                hit = p;
                hitEvent = PROCESS_WAIT_CONTINUED;
                return false;
            }
            return true;
        }, nullptr);
        
        if (hit != nullptr) {
            Event::UnregisterWaitNodes(&nodePtr, 1);

            if (wstatus != nullptr) {
                int status;
                if (hitEvent == PROCESS_WAIT_STOPPED)
                    status = (hit->GetStopSignal() << 8) | 0x7F;
                else if (hitEvent == PROCESS_WAIT_CONTINUED)
                    status = 0xFFFF;
                else
                    status = hit->GetExitStatus();
                if (!UserWrite(wstatus, &status, sizeof(int), currentProcess)) {
                    Scheduler::g_Processes.unlock();
                    return -EFAULT;
                }
            }
            
            int64_t reapedPid = hit->GetPID();
            if (hitEvent != 0) { // stop/continue don't reap
                hit->ClearWaitEvents(hitEvent);
                Scheduler::g_Processes.unlock();
                return reapedPid;
            }
            Scheduler::g_Processes.remove(hit);
            Scheduler::g_Processes.unlock();
            delete hit;
            return reapedPid;
        }
        
        Scheduler::g_Processes.unlock();
        
        if (!hasChildren) {
            Event::UnregisterWaitNodes(&nodePtr,1);
            return -ECHILD;
        }
            
        if (options & WNOHANG) {
            Event::UnregisterWaitNodes(&nodePtr,1);
            return 0;
        }
            
        // Block until awoken
        int rc = Event::BlockOnWaitNodes(&nodePtr, 1, UINT64_MAX);

        Event::UnregisterWaitNodes(&nodePtr, 1);
        
        if (rc == -EINTR)
            return -EINTR;
    }
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    if (pid < 0 || pgid < 0)
        return -EINVAL;

    Process* caller = Thread::GetCurrentThread()->GetParent();
    if (pid == 0)
        pid = caller->GetPID();
    if (pgid == 0)
        pgid = pid;

    Process* target;
    if (pid == caller->GetPID())
        target = caller;
    else {
        target = Scheduler::GetProcess(pid);
        if (target == nullptr || target->GetPPID() != caller->GetPID())
            return -ESRCH;
        if (target->HasExeced())
            return -EACCES;
    }

    if (target->GetSID() != caller->GetSID())
        return -EPERM;
    if (target->GetSID() == target->GetPID())
        return -EPERM; // session leader can't change group
    if (pgid != pid && !Scheduler::ProcessGroupExists(pgid, caller->GetSID()))
        return -EPERM; // can only join an existing group in the same session

    Scheduler::OrphanCandidate cands[16];
    int nc = Scheduler::SnapshotOrphanCandidates(target, cands, 16);
    target->SetPGID(pgid);
    Scheduler::CheckOrphanTransitions(cands, nc);
    return ESUCCESS;
}

pid_t sys_getpgid(pid_t pid) {
    Process* caller = Thread::GetCurrentThread()->GetParent();
    Process* target = (pid == 0 || pid == caller->GetPID()) ? caller : Scheduler::GetProcess(pid);
    if (target == nullptr)
        return -ESRCH;
    return target->GetPGID();
}

pid_t sys_setsid() {
    Process* proc = Thread::GetCurrentThread()->GetParent();
    pid_t pid = proc->GetPID();
    if (Scheduler::ProcessGroupExists(pid))
        return -EPERM;

    Scheduler::OrphanCandidate cands[16];
    int nc = Scheduler::SnapshotOrphanCandidates(proc, cands, 16);
    proc->SetSID(pid);
    proc->SetPGID(pid);
    proc->SetControllingTTY(nullptr);
    Scheduler::CheckOrphanTransitions(cands, nc);
    return pid;
}

pid_t sys_getsid(pid_t pid) {
    Process* caller = Thread::GetCurrentThread()->GetParent();
    Process* target = (pid == 0 || pid == caller->GetPID()) ? caller : Scheduler::GetProcess(pid);
    if (target == nullptr)
        return -ESRCH;
    return target->GetSID();
}
