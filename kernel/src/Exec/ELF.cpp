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

#define STACK_TOP_BUFFER 8

#ifndef __x86_64__
#error ELF format is only supported on x86_64
#endif

// Read a file, *data is allocated with new[], and must be freed by the caller
int ReadFile(const char* path, Process* proc, uint8_t** data, uint64_t* size) {
    const Credential& cred = proc->GetCred();
    FS::VNode* vnode = nullptr;
    int rc = FS::VFS_Open(path, &vnode, nullptr, cred);
    if (rc < 0)
        return rc;

    vnode->Lock();

    FS::VAttr attr{};
    rc = vnode->GetAttr(&attr);
    if (rc < 0) {
        vnode->Unlock();
        FS::VFS_Close(vnode, cred);
        return rc;
    }

    uint64_t fileSize = attr.size;
    if (fileSize == 0) {
        vnode->Unlock();
        FS::VFS_Close(vnode, cred);
        return -EINVAL;
    }

    uint8_t* buffer = new uint8_t[fileSize];
    if (buffer == nullptr) {
        vnode->Unlock();
        FS::VFS_Close(vnode, cred);
        return -ENOMEM;
    }

    uint64_t read = 0;
    rc = vnode->Read(buffer, fileSize, 0, 0, &read, cred);
    vnode->Unlock();
    if (rc < 0 || read != fileSize) {
        FS::VFS_Close(vnode, cred);
        delete[] buffer;
        if (rc == 0)
            return -ENOSYS; // amount read didn't match file size, but no error?
        return rc;
    }

    FS::VFS_Close(vnode, cred);

    *data = buffer;
    *size = fileSize;

    return ESUCCESS;
}

void HandleLoadFail(LinkedList::RearInsertLinkedList<Elf64_Phdr>& regions, VMM::VMM* vmm) {
    regions.EnumerateDelete([](Elf64_Phdr* phdr, void* data, uint64_t) -> LinkedList::IteratorDecision {
        VMM::VMM* vmm = static_cast<VMM::VMM*>(data);
        if (phdr->p_type == PT_LOAD && phdr->p_memsz > 0)
            vmm->FreePages(ALIGN_DOWN_ADDRESS(phdr->p_vaddr, PAGE_SIZE));
        return LinkedList::IteratorDecision::DELETE;
    }, vmm, 0);
}

int LoadELFFile(const char* path, Process* proc, void** entry, auxv64list_t* auxv64) {
    uint8_t* data = nullptr;
    uint64_t fileSize = 0;
    int rc = ReadFile(path, proc, &data, &fileSize);
    if (rc < 0)
        return rc;

    Elf64_Ehdr* header = (Elf64_Ehdr*)data;
    if (header->e_ident[EI_MAG0] != ELFMAG0 || header->e_ident[EI_MAG1] != ELFMAG1 || header->e_ident[EI_MAG2] != ELFMAG2 || header->e_ident[EI_MAG3] != ELFMAG3
        || header->e_ident[EI_CLASS] != ELFCLASS64 || header->e_ident[EI_DATA] != ELFDATA2LSB || header->e_ident[EI_OSABI] != ELFOSABI_SYSV
        || header->e_type != ET_EXEC || header->e_machine != EM_X86_64 || header->e_phoff == 0 || header->e_phnum == 0) {
        delete[] data;
        return -ENOEXEC;
    }

    auxv64->null.a_type = AT_NULL;
    auxv64->phdr.a_type = AT_PHDR;
    auxv64->phnum.a_type = AT_PHNUM;
    auxv64->phent.a_type = AT_PHENT;
    auxv64->entry.a_type = AT_ENTRY;
    auxv64->secure.a_type = AT_SECURE;
    auxv64->pagesz.a_type = AT_PAGESZ;

    auxv64->secure.a_val = 0; // set later
    auxv64->phnum.a_val = header->e_phnum;
    auxv64->phent.a_val = header->e_phentsize;
    auxv64->entry.a_val = header->e_entry;
    auxv64->pagesz.a_val = PAGE_SIZE;

    VMM::VMM* vmm = proc->GetVMM();

    LinkedList::RearInsertLinkedList<Elf64_Phdr> mappedRegions;

    Elf64_Phdr* phdr = (Elf64_Phdr*)((uint64_t)data + header->e_phoff);
    for (uint64_t i = 0; i < header->e_phnum; i++) {
        switch (phdr->p_type) {
        case PT_TLS:
        case PT_LOAD: {
            if (phdr->p_flags == 0 || phdr->p_flags > (PF_X | PF_W | PF_R) || phdr->p_memsz == 0) {
                HandleLoadFail(mappedRegions, vmm);
                return -ENOEXEC;
            }
            if (phdr->p_type == PT_TLS)
                phdr->p_vaddr = 0;

            void* pages = vmm->AllocateAnonPages(DIV_ROUNDUP(phdr->p_memsz, PAGE_SIZE), ALIGN_DOWN_ADDRESS(phdr->p_vaddr, PAGE_SIZE), VMM::DEFAULT_KALLOC_PHYS_FLAGS);
            if (pages == nullptr) {
                HandleLoadFail(mappedRegions, vmm);
                return -ENOMEM;
            }

            mappedRegions.insert(phdr);

            memcpy((void*)phdr->p_vaddr, (void*)((uint64_t)data + phdr->p_offset), phdr->p_filesz);

            VMM::Protection prot = VMM::Protection::NONE;

            switch(phdr->p_flags) {
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

            if (!vmm->RemapPages(pages, 0, prot, true, VMM::CacheType::DEFAULT)) {
                HandleLoadFail(mappedRegions, vmm);
                return -ENOMEM;
            }

            break;
        }
        case PT_INTERP:
            HandleLoadFail(mappedRegions, vmm);
            return -ENOSYS;
        case PT_PHDR:
            auxv64->phdr.a_val = phdr->p_vaddr;
        default:
            break;
        }

        phdr = (Elf64_Phdr*)((uint64_t)phdr + header->e_phentsize);
    }

    *entry = (void*)header->e_entry;

    mappedRegions.EnumerateDelete([](Elf64_Phdr*, void*, uint64_t) -> LinkedList::IteratorDecision {
        return LinkedList::IteratorDecision::DELETE;
    }, nullptr, 0);

    delete[] data;
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

    void* entry = nullptr;
    int rc = LoadELFFile(path, proc, &entry, &auxv64);
    if (rc < 0) {
        g_KPageMapper->SwapToThis();
        proc->Delete();
        delete proc;
        return rc;
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
