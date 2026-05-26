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

global x86_64_SyscallEntry
global x86_64_InitSyscall

extern x86_64_SyscallHandler

x86_64_SyscallEntry:
    swapgs
    mov QWORD [gs:80], rsp ; save user RSP
    mov rsp, QWORD [gs:24] ; get kernel RSP

    mov QWORD [gs:32], rax
    mov QWORD [gs:40], rbx
    mov QWORD [gs:48], rcx
    mov QWORD [gs:56], rdx
    mov QWORD [gs:64], rsi
    mov QWORD [gs:72], rdi
    ; skip rsp
    mov QWORD [gs:88], rbp
    mov QWORD [gs:96], r8
    mov QWORD [gs:104], r9
    mov QWORD [gs:112], r10
    mov QWORD [gs:120], r11
    mov QWORD [gs:128], r12
    mov QWORD [gs:136], r13
    mov QWORD [gs:144], r14
    mov QWORD [gs:152], r15
    mov QWORD [gs:160], rcx ; RIP
    mov QWORD [gs:168], r11 ; RFLAGS

    ; shift argument registers before using ax
    mov rcx, rdx ; c
    mov rdx, rsi ; b
    mov rsi, rdi ; a
    mov rdi, rax ; num

    mov ax, cs
    mov WORD [gs:176], ax

    mov ax, ss
    mov WORD [gs:178], ax

    xor rbp, rbp ; clear rbp
    cld

    

    call x86_64_SyscallHandler

    ; skip rax
    mov rbx, QWORD [gs:40]
    mov rcx, QWORD [gs:48]
    mov rdx, QWORD [gs:56]
    mov rsi, QWORD [gs:64]
    mov rdi, QWORD [gs:72]
    ; skip rsp
    mov rbp, QWORD [gs:88]
    mov r8, QWORD [gs:96]
    mov r9, QWORD [gs:104]
    mov r10, QWORD [gs:112]
    mov r11, QWORD [gs:120]
    mov r12, QWORD [gs:128]
    mov r13, QWORD [gs:136]
    mov r14, QWORD [gs:144]
    mov r15, QWORD [gs:152]

    mov QWORD [gs:24], rsp ; save kernel RSP
    mov rsp, QWORD [gs:80] ; get user RSP

    swapgs
    o64 sysret


x86_64_InitSyscall:
