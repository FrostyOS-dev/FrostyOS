; Copyright (©) 2024  Frosty515

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

global x86_64_EnsureNX
global x86_64_Ensure2MBPages
global x86_64_Ensure1GBPages

x86_64_EnsureNX:
    push rbp
    mov rbp, rsp

    mov rcx, 0xC0000080
    rdmsr
    and eax, 1<<11
    test eax, eax
    jz .disabled
    mov rax, 1
    jmp .end

.disabled:
    mov rax, 0x80000001
    xor rcx, rcx

    push rbx

    cpuid
    and edx, 1<<20
    test edx, edx
    jz .fail

    pop rbx

    mov rcx, 0xC0000080
    rdmsr
    or eax, 1<<11
    wrmsr
    mov rax, 1
    jmp .end

.fail:
    xor rax, rax

.end:
    mov rsp, rbp
    pop rbp
    ret

x86_64_Ensure2MBPages:
    push rbp
    mov rbp, rsp

    mov rax, cr4
    and rax, 1<<4
    test rax, rax
    jz .disabled
    mov rax, 1
    jmp .end

.disabled:
    mov rax, 0x1
    xor rcx, rcx

    push rbx

    cpuid
    and edx, 1<<3
    test edx, edx
    jz .fail

    pop rbx

    mov rax, cr4
    or rax, 1<<4
    mov cr4, rax
    mov rax, 1
    jmp .end

.fail:
    xor rax, rax

.end:
    mov rsp, rbp
    pop rbp
    ret

x86_64_Ensure1GBPages:
    push rbp
    mov rbp, rsp

    mov rax, 0x80000001
    xor rcx, rcx

    push rbx

    cpuid
    and edx, 1<<26
    test edx, edx
    jz .fail

    mov rax, 1
    jmp .end

.fail:
    xor rax, rax

.end:
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
