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

global x86_64_LoadGDT
x86_64_LoadGDT:
    lgdt [rdi]

    mov ax, 0x10 ; set the new data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pop rcx

    mov rax, 0x08 ; prepare new code segment
    push rax

    push rcx ; restore return address

    retfq ; far return into new code segment
