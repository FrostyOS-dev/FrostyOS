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

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>

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

    FS::VAttr attr{};
    rc = vnode->GetAttr(&attr);
    if (rc < 0) {
        FS::VFS_Close(vnode, cred);
        return rc;
    }

    uint64_t fileSize = attr.size;
    if (fileSize == 0) {
        FS::VFS_Close(vnode, cred);
        return -EINVAL;
    }

    uint8_t* buffer = new uint8_t[fileSize];
    if (buffer == nullptr) {
        FS::VFS_Close(vnode, cred);
        return -ENOMEM;
    }

    uint64_t read = 0;
    rc = vnode->Read(buffer, fileSize, 0, 0, &read, cred);
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
            vmm->FreePages(ALIGN_ADDRESS_DOWN(phdr->p_vaddr, PAGE_SIZE));
        return LinkedList::IteratorDecision::DELETE;
    }, vmm, 0);
}

int LoadELFFile(const char* path, Process* proc, void** entry) {
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

    VMM::VMM* vmm = proc->GetVMM();

    LinkedList::RearInsertLinkedList<Elf64_Phdr> mappedRegions;

    Elf64_Phdr* phdr = (Elf64_Phdr*)((uint64_t)data + header->e_phoff);
    for (uint64_t i = 0; i < header->e_phnum; i++) {
        switch (phdr->p_type) {
        case PT_LOAD: {
            if (phdr->p_flags == 0 || phdr->p_memsz == 0) {
                HandleLoadFail(mappedRegions, vmm);
                return -ENOEXEC;
            }
            void* pages = vmm->AllocatePages(DIV_ROUNDUP(phdr->p_memsz, PAGE_SIZE), ALIGN_ADDRESS_DOWN(phdr->p_vaddr, PAGE_SIZE), VMM::Protection::READ_WRITE, true, true);
            if (pages == nullptr) {
                dbgprintf("Failed to allocate region\n");
                HandleLoadFail(mappedRegions, vmm);
                return -ENOMEM;
            }

            mappedRegions.insert(phdr);

            memcpy((void*)phdr->p_vaddr, (void*)((uint64_t)data + phdr->p_offset), phdr->p_filesz);

            VMM::Protection prot;

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

            if (prot != VMM::Protection::READ_WRITE && !vmm->RemapMemory((uint64_t)pages, prot, true, VMM::CacheType::DEFAULT)) {
                HandleLoadFail(mappedRegions, vmm);
                return -ENOMEM;
            }

            break;
        }
        case PT_INTERP:
            HandleLoadFail(mappedRegions, vmm);
            return -ENOSYS;
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
