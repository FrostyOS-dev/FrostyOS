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

global GetCurrentProcessor
global GetCurrentProcessorState
global x86_64_SIMDInit

GetCurrentProcessor:
    mov rax, QWORD [gs:16]
    ret

GetCurrentProcessorState:
    mov rax, QWORD [gs:0]
    ret

x86_64_SIMDInit:
    mov rax, cr0
    and eax, ~4 ; clear EM
    or eax, 2 ; set MP
    mov cr0, rax

    mov rax, cr4
    or eax, 3 << 9 ; set OSFXSR and OSXMMEXCPT
    mov cr4, rax

    fninit

    mov rax, cr0
    or eax, 1 << 5 ; set NE
    mov cr0, rax

    test rdi, rdi
    jz .end

    mov rax, cr4
    or eax, 1 << 18 ; set OSXSAVE
    mov cr4, rax

    xor ecx, ecx
    mov eax, edi
    shr rdi, 32
    mov edx, edi
    xsetbv

.end:
    ret