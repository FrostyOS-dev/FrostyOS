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

global x86_64_InvalidatePage
global x86_64_FullTLBFlush
global x86_64_LoadCR3
global x86_64_GetCR3

x86_64_InvalidatePage:
    push rbp
    mov rbp, rsp ; not really needed, but for extra debug support on error

    invlpg [rdi]

    mov rsp, rbp
    pop rbp
    ret

x86_64_FullTLBFlush:
    push rbp
    mov rbp, rsp ; not really needed, but for extra debug support on error

    mov rax, cr3
    mov cr3, rax

    mov rsp, rbp
    pop rbp
    ret

x86_64_LoadCR3:
    push rbp
    mov rbp, rsp ; not really needed, but for extra debug support on error

    mov cr3, rdi

    mov rsp, rbp
    pop rbp
    ret

x86_64_GetCR3:
    mov rax, cr3
    ret
