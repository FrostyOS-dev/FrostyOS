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

[[noreturn]] void sys_exit(uint64_t code) {
    Thread::ExitCurrentThread(code, true, true, true);

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
    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    return proc->GetPID(); // todo: unique TIDs between processes
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

    if (!FDManager->Fork(currentProc->GetFDManager(), proc)) {
        delete FDManager;
        delete proc;
        return -ENOMEM;
    }

    proc->SetFDManager(FDManager);
    proc->SetCred(currentProc->GetCred());
    proc->SetCWD(currentProc->GetCWD());

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
            currentArgv = (char**)krealloc(currentArgv, maxArgc * sizeof(char*));
            if (currentArgv == nullptr)
                return -ENOMEM;
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
            currentEnv = (char**)krealloc(currentEnv, maxEnvc * sizeof(char*));
            if (currentEnv == nullptr) {
                kfree(currentEnv);
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

    rc = CreateELFProcess(kPath, parent, kArgv, kEnv, true, &newProc);

    delete kPath;
    CleanupArgEnv(argc, argc, kArgv, envc, envc, kEnv);

    if (rc < 0)
        return rc;

    newProc->SetPID(currentProc->GetPID());
    newProc->SetPPID(parentPID);
    newProc->SetCWD(currentProc->GetCWD());
    newProc->SetCred(currentProc->GetCred());

    FileDescriptorManager* FDManager = newProc->GetFDManager();
    FDManager->Delete();
    FDManager->Fork(currentProc->GetFDManager(), newProc);

    Scheduler::g_Processes.lock();
    Scheduler::g_Processes.remove(currentProc);
    Scheduler::g_Processes.unlock();

    if (!newProc->Start()) {
        newProc->Delete();
        delete newProc;
    }

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
        Process* zombieChild = nullptr;
        
        Scheduler::g_Processes.lock();
        Scheduler::g_Processes.Enumerate([&](Process* p, void* data) -> bool {
            if (p->GetPPID() == currentProcess->GetPID() && (pid == -1 || p->GetPID() == pid)) {
                hasChildren = true;
                if (p->GetState() == ProcessState::ZOMBIE) {
                    zombieChild = p;
                    return false; // Break enumeration
                }
            }
            return true;
        }, nullptr);
        
        if (zombieChild != nullptr) {
            Event::UnregisterWaitNodes(&nodePtr, 1);

            if (wstatus != nullptr) {
                int status = zombieChild->GetExitStatus();
                if (!UserWrite(wstatus,  &status, sizeof(int), currentProcess, true)) {
                    Scheduler::g_Processes.unlock();
                    return -EFAULT;
                }
            }
            
            uint64_t reapedPid = zombieChild->GetPID();
            Scheduler::g_Processes.remove(zombieChild);
            Scheduler::g_Processes.unlock();
            
            delete zombieChild; // Final cleanup
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
            
        // If awoken (rc > 0), the loop repeats to find the new zombie
    }
}

