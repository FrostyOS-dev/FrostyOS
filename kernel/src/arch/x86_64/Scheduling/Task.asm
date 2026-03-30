; Copyright (©) 2024-2026  Frosty515

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

extern x86_64_WriteMSR
extern Scheduler_YieldAfterSave

global x86_64_KernelSwitchTask
global x86_64_PrepCurrentThreadExit
global x86_64_Halt
global Scheduler_SaveAndYield

x86_64_KernelSwitchTask:
    cli ; make sure interrupts are disabled

    ; as many GPRs as possible
    mov rbx, QWORD [rdi+0x08] ; skip rax
    mov rcx, QWORD [rdi+0x10]
    mov rdx, QWORD [rdi+0x18]
    mov rsi, QWORD [rdi+0x20]
    mov rbp, QWORD [rdi+0x38] ; skip rdi and rsp
    mov r8,  QWORD [rdi+0x40]
    mov r9,  QWORD [rdi+0x48]
    mov r10, QWORD [rdi+0x50]
    mov r11, QWORD [rdi+0x58]
    mov r12, QWORD [rdi+0x60]
    mov r13, QWORD [rdi+0x68]
    mov r14, QWORD [rdi+0x70]
    mov r15, QWORD [rdi+0x78]

    ; CR3
    mov rax, QWORD [rdi+0x90]
    mov cr3, rax

    ; RSP
    mov rsp, QWORD [rdi+0x30]

    ; prepare RIP
    push QWORD [rdi+0x80]

    ; prepare RFLAGS
    push QWORD [rdi+0x88]

    ; RAX+RDI
    mov rax, QWORD [rdi]
    mov rdi, QWORD [rdi+0x28]

    ; set RFLAGS
    popf

    ; return into new code
    ret


; rdi = thread, rsi = newStack, rdx = func, rcx = arg
x86_64_PrepCurrentThreadExit:
    push rbp
    push r10
    mov r10, rsp ; use a callee-saved register
    mov rsp, rsi
    mov rsi, rcx
    xor rbp, rbp
    call rdx
    mov rsp, r10
    pop r10
    pop rbp
    ret

x86_64_Halt:
    cli
.l:
    hlt
    jmp .l

Scheduler_SaveAndYield:
    pushf
    cli
    pop rcx ; save for later
    pop r8 ; return address
    mov r9, rsp

    ; caller-saved registers don't need to be saved in this case
    xor rax, rax
    mov ax, cs
    mov dx, ss
    shl edx, 16
    or eax, edx
    push rax ; CS, SS and alignment

    mov rax, cr3
    push rax ; CR3
    push rcx ; RFLAGS
    push r8 ; RIP
    push r15
    push r14
    push r13
    push r12
    sub rsp, 32 ; ignore r8-r11
    push rbp
    push r9 ; RSP
    sub rsp, 32 ; ignore rcx, rdx, rsi, rdi
    push rbx
    sub rsp, 16 ; ignore rax and align the stack

    ; The 16 bytes reserved above consist of an unused "rax slot" plus padding
    ; to keep the stack 16-byte aligned. Skip the lowest 8 bytes of this padding
    ; so RSI points at the first field of the saved register frame structure.
    ; The value of RDI is just parsed on.
    lea rsi, [rsp + 8]
    call Scheduler_YieldAfterSave

.l:
    hlt
    jmp .l
