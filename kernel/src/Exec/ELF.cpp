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

#include "ELF.hpp"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

#include <fs/VFS.hpp>

#include <DataStructures/LinkedList.hpp>

#include <Memory/PageMapper.hpp>
#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

#include <SystemCalls/Memory.hpp>

#define STACK_TOP_BUFFER 8

#ifndef __x86_64__
#error ELF format is only supported on x86_64
#endif

int ReadExact(FS::VNode* vnode, void* buf, size_t size, size_t offset, Credential cred) {
    size_t bytesRead = 0;
    int rc = vnode->Read(buf, size, 0, offset, &bytesRead, cred);
    if (rc < 0 || bytesRead != size)
        return rc == 0 ? -EINVAL : rc;
    return 0;
}

int LoadELFFile(const char* path, void* base, Process* proc, void** entry, auxv64list_t* auxv64, char** interp) {
    const Credential& cred = proc->GetCred();
    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_Open(path, &vnode, nullptr, cred);
    if (rc < 0)
        return rc;

    vnode->Lock();

    Elf64_Ehdr header;

    rc = ReadExact(vnode, &header, sizeof(Elf64_Ehdr), 0, cred);
    if (rc < 0) {
        FS::VFS_Close(vnode, cred);
        return rc;
    }

    if (header.e_ident[EI_MAG0] != ELFMAG0 || header.e_ident[EI_MAG1] != ELFMAG1 || header.e_ident[EI_MAG2] != ELFMAG2 || header.e_ident[EI_MAG3] != ELFMAG3
        || header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_OSABI] != ELFOSABI_SYSV
        || (header.e_type != ET_EXEC && header.e_type != ET_DYN) || header.e_machine != EM_X86_64 || header.e_phoff == 0 || header.e_phnum == 0) {
        FS::VFS_Close(vnode, cred);
        dbgprintf("bad header\n");
        return -ENOEXEC;
    }

    if (base == nullptr && header.e_type == ET_DYN)
        base = (void*)0x400000;

    auxv64->null.a_type = AT_NULL;
    auxv64->phdr.a_type = AT_PHDR;
    auxv64->phnum.a_type = AT_PHNUM;
    auxv64->phent.a_type = AT_PHENT;
    auxv64->entry.a_type = AT_ENTRY;
    auxv64->secure.a_type = AT_SECURE;
    auxv64->pagesz.a_type = AT_PAGESZ;

    auxv64->secure.a_val = 0; // set later
    auxv64->phnum.a_val = header.e_phnum;
    auxv64->phent.a_val = header.e_phentsize;
    auxv64->entry.a_val = header.e_entry;
    auxv64->pagesz.a_val = PAGE_SIZE;

    VMM::VMM* vmm = proc->GetVMM();

    Elf64_Phdr interpPhdr;
    bool hasInterp = false;

    
    for (uint64_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        rc = ReadExact(vnode, &phdr, sizeof(Elf64_Phdr), header.e_phoff + i * header.e_phentsize, cred);
        if (rc < 0) {
            FS::VFS_Close(vnode, cred);
            return rc;
        }
        phdr.p_vaddr += (uint64_t)base;
        switch (phdr.p_type) {
        case PT_TLS:
        case PT_LOAD: {
            if (phdr.p_flags == 0 || phdr.p_flags > (PF_X | PF_W | PF_R) || phdr.p_memsz == 0 || phdr.p_vaddr % PAGE_SIZE != phdr.p_offset % PAGE_SIZE) {
                vnode->Unlock();
                FS::VFS_Close(vnode, cred);
                dbgprintf("bad phdr\n");
                return -ENOEXEC;
            }
            if (phdr.p_type == PT_TLS)
                phdr.p_vaddr = 0;

            VMM::Protection prot = VMM::Protection::NONE;

            switch(phdr.p_flags) {
            case PF_X:
                prot = VMM::Protection::EXECUTE;
                break;
            case PF_W:
                prot = VMM::Protection::WRITE;
                break;
            case PF_X | PF_W:
                prot = VMM::Protection::READ_WRITE_EXECUTE;
                break;
            case PF_R:
                prot = VMM::Protection::READ;
                break;
            case PF_X | PF_R:
                prot = VMM::Protection::READ_EXECUTE;
                break;
            case PF_W | PF_R:
                prot = VMM::Protection::READ_WRITE;
                break;
            case PF_X | PF_W | PF_R:
                prot = VMM::Protection::READ_WRITE_EXECUTE;
                break;
            }

            void* pages = (void*)phdr.p_vaddr;

            uint64_t firstPageOffset = phdr.p_vaddr % PAGE_SIZE;

            // Need to map in 4 sections: unaligned start, aligned middle, unaligned end, difference between p_memsz and p_filesz
            if (firstPageOffset > 0) {
                size_t firstPageCount = MIN(PAGE_SIZE - firstPageOffset, phdr.p_filesz);
                if (nullptr == vmm->AllocateAnonPages(1, ALIGN_DOWN_ADDRESS(pages, PAGE_SIZE), VMM::DEFAULT_KALLOC_PHYS_FLAGS)) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    return -ENOMEM;
                }

                rc = ReadExact(vnode, pages, firstPageCount, phdr.p_offset, cred);
                if (rc < 0) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    return rc;
                }

                phdr.p_vaddr += firstPageCount;
                phdr.p_memsz -= firstPageCount;
                phdr.p_filesz -= firstPageCount;
                phdr.p_offset += firstPageCount;

                size_t remaining = PAGE_SIZE - (phdr.p_vaddr % PAGE_SIZE);
                if (remaining > 0 && phdr.p_memsz > 0) {
                    size_t remMSize = MIN(phdr.p_memsz, remaining);
                    size_t pageDiff = ALIGN_UP(phdr.p_vaddr, PAGE_SIZE) - phdr.p_memsz;
                    if (remMSize > pageDiff)
                        remMSize = pageDiff;

                    phdr.p_memsz -= remMSize;
                    phdr.p_vaddr += remMSize;
                }

                // no need to do any zeroing as the page is already zeroed by the VMM


                if (!vmm->RemapPages(ALIGN_DOWN_ADDRESS(pages, PAGE_SIZE), 0, prot, true, VMM::CacheType::DEFAULT)) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    dbgprintf("remap error\n");
                    return -ENOEXEC;
                }
            }

            uint64_t byteSize = ALIGN_DOWN(phdr.p_filesz, PAGE_SIZE);
            if (byteSize > 0) { // Don't need to remap this section as no kernel-mode writes occur
                void* mem = nullptr;
                rc = FS::VFS_MapFile((void*)phdr.p_vaddr, byteSize, prot, MAP_PRIVATE | MAP_FIXED, true, vnode, phdr.p_offset, &mem, vmm, cred);
                if (rc < 0 || mem == nullptr) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    return rc < 0 ? rc : -ENOSYS;
                }
            
                if (phdr.p_type == PT_TLS)
                    phdr.p_vaddr = (uint64_t)mem;

                phdr.p_vaddr += byteSize;
                phdr.p_memsz -= byteSize;
                phdr.p_filesz -= byteSize;
                phdr.p_offset += byteSize;
            }

            uint64_t lastPageCount = phdr.p_filesz % PAGE_SIZE;
            if (lastPageCount > 0) {
                if (nullptr == vmm->AllocateAnonPages(1, (void*)phdr.p_vaddr, VMM::DEFAULT_KALLOC_PHYS_FLAGS)) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    return -ENOMEM;
                }

                rc = ReadExact(vnode, (void*)phdr.p_vaddr, lastPageCount, phdr.p_offset, cred);
                if (rc < 0) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    return rc;
                }

                if (!vmm->RemapPages((void*)phdr.p_vaddr, 0, prot, true, VMM::CacheType::DEFAULT)) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    dbgprintf("remap error 2\n");
                    return -ENOEXEC;
                }

                phdr.p_memsz -= MIN(phdr.p_memsz, PAGE_SIZE);
                phdr.p_vaddr += PAGE_SIZE;
            }

            if (phdr.p_memsz > 0) {
                if (nullptr == vmm->AllocateAnonPages(DIV_ROUNDUP(phdr.p_memsz, PAGE_SIZE), (void*)ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE), VMM::DEFAULT_ALLOC_FLAGS)) {
                    vnode->Unlock();
                    FS::VFS_Close(vnode, cred);
                    dbgprintf("memsz align error: count = %lx, addr = %p\n", DIV_ROUNDUP(phdr.p_memsz, PAGE_SIZE), (void*)ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE));
                    return -ENOEXEC;
                }
            }

            break;
        }
        case PT_INTERP:
            memcpy(&interpPhdr, &phdr, sizeof(Elf64_Phdr));
            hasInterp = true;
            break;
        case PT_PHDR:
            auxv64->phdr.a_val = phdr.p_vaddr;
        default:
            break;
        }
    }

    if (hasInterp) {
        char* buf = (char*)kmalloc(interpPhdr.p_filesz);
        if (buf == nullptr) {
            vnode->Unlock();
            FS::VFS_Close(vnode, cred);
            return -ENOMEM;
        }
        rc = ReadExact(vnode, buf, interpPhdr.p_filesz, interpPhdr.p_offset, cred);
        if (rc < 0) {
            delete buf;
            vnode->Unlock();
            FS::VFS_Close(vnode, cred);
            return rc;
        }
        *interp = buf;
    }

    *entry = (void*)(header.e_entry + (uint64_t)base);

    vnode->Unlock();
    FS::VFS_Close(vnode, cred);
    return ESUCCESS;
}

void* PrepareELFStack(void* stackTop, auxv64list_t* auxv64, char** argv, char** env, const char* path, VMM::VMM* vmm) {
    if (stackTop == nullptr || auxv64 == nullptr || argv == nullptr || env == nullptr || path == nullptr || vmm == nullptr)
        return nullptr;
    size_t argc;
    size_t envc;
    size_t argDataSize = 0;
    size_t envDataSize = 0;
    size_t pathSize = strlen(path) + 1;

    for (argc = 0; argv[argc] != nullptr; argc++)
        argDataSize += strlen(argv[argc]) + 1;

    for (envc = 0; env[envc] != nullptr; envc++)
        envDataSize += strlen(env[envc]) + 1;

    // Ensure the stack is aligned
    uint8_t align = (argc + envc + 3) & 1 ? 8 : 0;

    size_t size = argDataSize + envDataSize + pathSize + (argc + envc) * sizeof(char*) + sizeof(size_t) + sizeof(auxv64list_t) + align + STACK_TOP_BUFFER;
    void* base = (void*)((uint64_t)stackTop - size);
    if (!vmm->MapPages(ALIGN_DOWN_ADDRESS(base, PAGE_SIZE), DIV_ROUNDUP(size, PAGE_SIZE)))
        return nullptr;

    stackTop = (void*)((uint64_t)stackTop - STACK_TOP_BUFFER);

    char* argDataStart = (char*)((uint64_t)stackTop - argDataSize);
    char* envDataStart = (char*)((uint64_t)argDataStart - envDataSize);
    char* pathDataStart = (char*)((uint64_t)envDataStart - pathSize);
    auxv64list_t* auxvStart = (auxv64list_t*)pathDataStart - 1;

    // Align the stack after strings for the auxv list
    auxvStart = (auxv64list_t*)(ALIGN_DOWN_BASE2((uint64_t)auxvStart, 16) - align);

    char** envStart = (char**)auxvStart - envc - 1;
    char** argStart = (char**)envStart - argc - 1;
    uint64_t* argcPoint = (uint64_t*)argStart - 1;

    auxv64->execfn.a_type = AT_EXECFN;
    auxv64->execfn.a_val = (uint64_t)pathDataStart;
    
    // TODO: user access begin?
    memcpy(pathDataStart, path, pathSize);

    memcpy(auxvStart, auxv64, sizeof(auxv64list_t));

    for (uint64_t i = 0; i < argc; i++) {
        strcpy(argDataStart, argv[i]);
        argStart[i] = argDataStart;
        argDataStart += strlen(argv[i]) + 1;
    }

    for (uint64_t i = 0; i < envc; i++) {
        strcpy(envDataStart, env[i]);
        envStart[i] = envDataStart;
        envDataStart += strlen(env[i]) + 1;
    }

    argStart[argc] = nullptr;
    envStart[envc] = nullptr;
    *argcPoint = argc;

    // TODO: user access end?
    return argcPoint;
}

int CreateELFProcess(const char* path, Process* parent, char** argv, char** env) {
    Process* proc = new Process(ProcessMode::USER, nullptr, 15);
    if (!proc->Create()) {
        delete proc;
        return -ENOMEM;
    }

    if (parent != nullptr)
        proc->SetPPID(parent->GetPID());

    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr || vmm->GetPageMapper() == nullptr) {
        proc->Delete();
        delete proc;
        return -ENOSYS;
    }

    PageMapper* mapper = vmm->GetPageMapper();
    if (!mapper->SwapToThis()) {
        proc->Delete();
        delete proc;
        return -ENOSYS;
    }

    auxv64list_t auxv64;
    memset(&auxv64, 0, sizeof(auxv64list_t));

    char* interp = nullptr;
    void* entry = nullptr;
    int rc = LoadELFFile(path, nullptr, proc, &entry, &auxv64, &interp);
    if (rc < 0) {
        g_KPageMapper->SwapToThis();
        proc->Delete();
        delete proc;
        return rc;
    }

    if (interp != nullptr) {
        auxv64list_t interpauxv64;
        memset(&interpauxv64, 0, sizeof(auxv64list_t));
        char* interpinterp = nullptr;
        rc = LoadELFFile(interp, INTERP_BASE, proc, &entry, &interpauxv64, &interpinterp);
        if (rc < 0 || interpinterp != nullptr) {
            g_KPageMapper->SwapToThis();
            proc->Delete();
            delete proc;
            dbgprintf("interpinterp = %p\n", interpinterp);
            return interpinterp == nullptr ? rc : -ENOEXEC;
        }
        delete interp;
    }

    if (!proc->CreateMainThread({(void (*)(void*))entry, nullptr})) {
        g_KPageMapper->SwapToThis();
        proc->Delete();
        delete proc;
        return -ENOMEM;
    }

    Thread* thread = proc->GetMainThread();
    void* stack = PrepareELFStack((void*)thread->GetStack(), &auxv64, argv, env, path, vmm);
    if (stack == nullptr) {
        g_KPageMapper->SwapToThis();
        proc->Delete();
        delete proc;
        return -ENOMEM;
    }

    thread->SetStack((uint64_t)stack);

    rc = ESUCCESS;

    if (!proc->Start()) {
        proc->Delete();
        delete proc;
        rc = -ENOSYS;
    }

    g_KPageMapper->SwapToThis();
    return rc;
}
