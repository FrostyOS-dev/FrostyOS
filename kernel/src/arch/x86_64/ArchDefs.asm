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

global x86_64_DisableInterrupts
global x86_64_EnableInterrupts
global x86_64_DisableInterruptsWithSave
global x86_64_EnableInterruptsWithSave

x86_64_DisableInterrupts:
    cli
    ret

x86_64_EnableInterrupts:
    sti
    ret

x86_64_DisableInterruptsWithSave:
    push rbp
    mov rbp, rsp

    pushf
    pop rax

    cli

    mov rsp, rbp
    pop rbp
    ret

x86_64_EnableInterruptsWithSave:
    push rbp
    mov rbp, rsp

    push rdi
    popf

    sti

    mov rsp, rbp
    pop rbp
    ret

global x86_64_GetLAPICID

x86_64_GetLAPICID:
    push rbp
    mov rbp, rsp

    push rbx

    mov eax, 0x01
    cpuid

    shr ebx, 24
    movzx eax, bl

    pop rbx

    mov rsp, rbp
    pop rbp
    ret
