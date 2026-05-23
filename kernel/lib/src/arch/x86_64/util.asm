; Copyright (©) 2023-2026  Frosty515
; 
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
; 
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
; 
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <https://www.gnu.org/licenses/>.

[bits 64]

global memset
memset:
    push rbp
    mov rbp, rsp

    push rdi

    mov al, sil
    mov rcx, rdx
    rep stosb

    pop rax

    mov rsp, rbp
    pop rbp
    ret

global memcpy
memcpy:
    push rbp
    mov rbp, rsp

    mov rax, rdi
    mov rcx, rdx
    rep movsb

    mov rsp, rbp
    pop rbp
    ret

global memcmp_b
memcmp_b:
    push rbp
    mov rbp, rsp

    xor rcx, rcx

    cmp rcx, rdx
    je .success

.l:
    cmp BYTE [rdi+rcx], sil
    jne .fail
    add rcx, 1
    cmp rdx, rcx
    jne .l

.success:
    mov rax, 1
    jmp .end

.fail:
    xor rax, rax

.end:
    mov rsp, rbp
    pop rbp
    ret
