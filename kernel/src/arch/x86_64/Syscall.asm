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

global x86_64_SyscallEntry
global x86_64_InitSyscall

extern x86_64_SyscallHandler

x86_64_SyscallEntry:
    swapgs
    mov QWORD [gs:80], rsp ; save user RSP
    mov rsp, QWORD [gs:24] ; get kernel RSP

    push QWORD 0x1b0023 ; CS, SS, _align
    push 0 ; fill later with cr3

    push r11 ; RFLAGS
    push rcx ; RIP
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push QWORD [gs:80] ; rsp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr3
    mov QWORD [rsp + 144], rax

    mov rdi, rsp ; register frame
    xor rbp, rbp ; clear rbp
    cld

    call x86_64_SyscallHandler

    cli ; need to disable them again

    mov rcx, QWORD [rsp + 144] ; cr3 while we still have available GPRs
    mov cr3, rcx

    add rsp, 8 ; ignore rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop QWORD [gs:80] ; rsp for later
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    pop rcx ; RIP
    pop r11 ; RFLAGS

    add rsp, 16 ; ignore segments, alignment and cr3

    mov QWORD [gs:24], rsp ; save kernel RSP
    mov rsp, QWORD [gs:80] ; get user RSP

    swapgs
    o64 sysret


x86_64_InitSyscall:
