; Copyright (©) 2024-2025  Frosty515

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

global x86_64_Internal_Is5LevelPagingSupported
global x86_64_EnsureNXSupport
global x86_64_Ensure2MiBPageSupport
global x86_64_Ensure1GiBPageSupport
global x86_64_LoadCR3
global x86_64_InvalidatePage
global x86_64_FlushTLB

x86_64_Internal_Is5LevelPagingSupported:
    push rbp
    mov rbp, rsp

    push rbx ; save rbx

    mov eax, 7
    xor ecx, ecx
    cpuid
    and ecx, 1<<16
    cmp ecx, 1<<16
    sete al
    
    pop rbx

    mov rsp, rbp
    pop rbp
    ret

x86_64_EnsureNXSupport:
    push rbp
    mov rbp, rsp

    ; first check if it is enabled in EFER
    mov ecx, 0xC0000080
    rdmsr
    and eax, 1<<11
    cmp eax, 1<<11
    je .success

    ; it isn't enabled, so check if it is supported
    push rbx ; save rbx

    mov eax, 0x80000001
    xor ecx, ecx
    cpuid

    pop rbx ; restore rbx

    and edx, 1<<20
    cmp edx, 1<<20
    jne .fail

    ; it is supported, so now enable it
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1<<11
    wrmsr

.success:
    mov al, 1
    jmp .end

.fail:
    xor al, al

.end:
    mov rsp, rbp
    pop rbp
    ret

x86_64_Ensure2MiBPageSupport:
    push rbp
    mov rbp, rsp

    ; first check cr4 to see if it is enabled
    mov rax, cr4
    and eax, 1<<4
    cmp eax, 1<<4
    je .success ; already enabled

    ; it isn't enabled, so check if it exists
    push rbx ; save rbx

    mov eax, 1
    xor ecx, ecx
    cpuid

    pop rbx ; restore rbx

    and edx, 1<<3
    cmp edx, 1<<3
    jne .fail

    ; it is supported, so enable it

    mov rax, cr4
    or rax, 1<<4
    mov cr4, rax

.success:
    mov rax, 1
    jmp .end

.fail:
    xor rax, rax

.end:
    mov rsp, rbp
    pop rbp
    ret

x86_64_Ensure1GiBPageSupport:
    push rbp
    mov rbp, rsp

    push rbx ; save rbx

    mov eax, 0x80000001
    xor ecx, ecx
    cpuid
    and edx, 1<<26
    cmp edx, 1<<26
    sete al

    pop rbx ; restore rbx

    mov rsp, rbp
    pop rbp
    ret

x86_64_LoadCR3:
    mov cr3, rdi
    ret

x86_64_InvalidatePage:
    invlpg [rdi]
    ret

x86_64_FlushTLB:
    mov rax, cr3
    mov cr3, rax
    ret
