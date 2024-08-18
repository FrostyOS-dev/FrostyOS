; Copyright (©) 2023-2024  Frosty515
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

global div
div:
    xor edx, edx
    mov eax, edi
    idiv esi
    shl rdx, 32
    or rax, rdx
    ret

global udiv
udiv:
    xor edx, edx
    mov eax, edi
    div esi
    shl rdx, 32
    or rax, rdx
    ret

global ldiv
ldiv:
    xor rdx, rdx
    mov rax, rdi
    idiv rsi
    ret

global uldiv
uldiv:
    xor rdx, rdx
    mov rax, rdi
    div rsi
    ret

global log2
log2:
    bsr rax, rdi
    ret

global get_lsb
get_lsb:
    bsf rax, rdi
    ret
