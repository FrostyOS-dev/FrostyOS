; Copyright (©) 2025  Frosty515

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

global x86_64_ReadMSR
global x86_64_WriteMSR

x86_64_ReadMSR:
    push rbp
    mov rbp, rsp

    xor rax, rax

    mov ecx, edi
    rdmsr

    shl rdx, 32
    or rax, rdx

    mov rsp, rbp
    pop rbp
    ret

x86_64_WriteMSR:
    push rbp
    mov rbp, rsp

    mov ecx, edi
    mov eax, esi
    shr rsi, 32
    mov edx, esi
    wrmsr

    mov rsp, rbp
    pop rbp
    ret
