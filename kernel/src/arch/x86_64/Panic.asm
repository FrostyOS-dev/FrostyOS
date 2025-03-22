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

extern x86_64_Panic

global x86_64_PrePanic
x86_64_PrePanic:
    push rbp
    mov rbp, rsp
    pushf
    cli
    push rax
    sub rsp, 6 
    mov WORD [rsp], ss
    sub rsp, 2
    mov WORD [rsp], cs
    mov rax, cr3
    push rax
    push QWORD [rbp-8] ; RFLAGS
    push QWORD [rbp+8] ; RIP
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push QWORD [rbp] ; get rbp
    lea rax, [rbp-24]
    push rax
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push QWORD [rbp-16]
    xor rdi, rdi
    mov rsi, rsp
    xor rdx, rdx
    call x86_64_Panic
.l: ; should be unreachable, but here just in case
    hlt
    jmp .l
