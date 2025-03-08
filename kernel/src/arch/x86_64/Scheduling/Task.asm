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

extern x86_64_WriteMSR

global x86_64_KernelSwitchTask
global x86_64_SwitchTask

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
