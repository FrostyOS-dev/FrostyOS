; Copyright (©) 2026  Frosty515

; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.

; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.

; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <https://www.gnu.org/licenses/>.

[bits 64]

section .data

     cr3_loc equ 0xF80
     cr4_loc equ 0xF84
stackEnd_loc equ 0xF88
    func_loc equ 0xF90
    proc_loc equ 0xF98
    GDTR_loc equ 0xFA0
     KCS_loc equ 0xFAA
     KDS_loc equ 0xFAC
    IDTR_loc equ 0xFAE

global x86_64_APTrampoline
align 0x1000
x86_64_APTrampoline:
    [bits 16]
    cld
    cli
    jmp near .realmode - x86_64_APTrampoline
align 0x10
.gdt:
    dq 0 ; null
    dq 0x00AF9A000000FFFF ; 64-bit code
    dq 0x00AF92000000FFFF ; 64-bit data
.gdtr:
    dw 0x17
    dq .gdt - x86_64_APTrampoline
align 64
.realmode:
    lgdt [.gdtr - x86_64_APTrampoline]

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, 0x30 ; PAE, PSE
    or eax, DWORD [ds:cr4_loc]
    mov cr4, eax

    mov eax, DWORD [ds:cr3_loc]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x900 ; LME, NXE
    wrmsr

    mov eax, 0x80001001 ; Activate long mode - PG, WP, PE
    mov cr0, eax

    lgdt [.gdtr - x86_64_APTrampoline]

    jmp 8:.longmode - x86_64_APTrampoline

.longmode:
    [bits 64]

    lgdt [abs GDTR_loc]

    mov ax, WORD [abs KDS_loc]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, QWORD [abs stackEnd_loc]
    xor rbp, rbp
    
    movzx rax, WORD [abs KCS_loc]
    push rax

    push .newGDT - x86_64_APTrampoline

    retfq

.newGDT:
    lidt [abs IDTR_loc]

    mov rdi, QWORD [abs proc_loc]

    push 0

    mov rax, QWORD [abs func_loc]
    jmp rax

align 0x1000
.end:
