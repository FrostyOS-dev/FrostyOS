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

#ifndef _ELF_HPP
#define _ELF_HPP

#include <stdint.h>

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_PAD 8
#define EI_NIDENT 16

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64 2

#define ELFDATA2LSB 1

#define ELFOSABI_NONE 0
#define ELFOSABI_SYSV 0

#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

#define EM_NONE   0x00
#define EM_X86_64 0x3E

#define PT_NULL                      0
#define PT_LOAD                      1
#define PT_DYNAMIC                   2
#define PT_INTERP                    3
#define PT_NOTE                      4
#define PT_SHLIB                     5
#define PT_PHDR                      6
#define PT_TLS                       7
#define PT_NUM                       8
#define PT_LOOS             0x60000000
#define PT_GNU_EH_FRAME     0x6474E550
#define PT_GNU_STACK        0x6474E551
#define PT_GNU_RELRO        0x6474E552
#define PT_GNU_PROPERTY     0x6474E553
#define PT_SUNWBSS          0x6ffffffa
#define PT_SUNWSTACK        0x6ffffffb
#define PT_HISUNW           0x6fffffff
#define PT_HIOS             0x6fffffff
#define PT_LOPROC           0x70000000
#define PT_HIPROC           0x7fffffff

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_SECURE 23
#define AT_EXECFN 31


struct Elf64_Ehdr {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct auxv64_t {
    uint64_t a_type;
    uint64_t a_val;
};

struct auxv64list_t {
    auxv64_t phdr;
    auxv64_t phnum;
    auxv64_t phent;
    auxv64_t entry;
    auxv64_t execfn;
    auxv64_t secure;
    auxv64_t pagesz;
    auxv64_t null;
};

class Process;

int LoadELFFile(const char* path, Process* proc, void** entry, auxv64list_t* auxv64);

int CreateELFProcess(const char* path, Process* parent, char** argv, char** env);

#endif /* _ELF_HPP */