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

section .data

; 0xFFC = CR3

global x86_64_AP_trampoline
align 0x1000
x86_64_AP_trampoline:
    [bits 16]
    cld
    cli
    jmp near .realmode - x86_64_AP_trampoline
align 0x10
.gdt:
    dq 0x0000000000000000 ; null
    ; dq 0x00009A000000FFFF ; 16-bit code
    ; dq 0x000092000000FFFF ; 16-bit data
    ; dq 0x00CF9A000000FFFF ; 32-bit code
    ; dq 0x00CF92000000FFFF ; 32-bit data
    dq 0x00AF9A000000FFFF ; 64-bit code
    dq 0x00AF92000000FFFF ; 64-bit data
.gdtr:
    dw 0x17
    dq .gdt - x86_64_AP_trampoline
.realmode:
    mov eax, 0x30 ; set PAE and PSE
    mov cr4, eax

    mov eax, DWORD [0xFFC] ; set CR3
    mov cr3, eax

    mov ecx, 0xC0000080 ; read EFER MSR
    rdmsr
    or eax, 0x100 ; set LME
    wrmsr

    mov eax, 0x80001001 ; Activate long mode by enabling paging an protection simultaneously whilst also setting WP
    mov cr0, eax

    lgdt [.gdtr - x86_64_AP_trampoline]

    jmp 0x08:longmode - x86_64_AP_trampoline

longmode:
    mov al, 'Y'
    out 0xE9, al
.l:
    hlt
    jmp .l

align 0x1000
.end:
