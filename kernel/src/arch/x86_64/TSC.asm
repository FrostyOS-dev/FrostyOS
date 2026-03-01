; Copyright (©) 2026  Frosty515

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

; functions from header

global x86_64_ReadTSC
global x86_64_ReadTSCFence

x86_64_ReadTSC:
    rdtsc ; EDX:EAX = TSC value. higher 32 bits of each are cleared
    shl rdx, 32
    or rax, rdx
    ret

x86_64_ReadTSCFence:
    mfence
    lfence
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret

; functions from source file

global x86_64_TSCEnable

x86_64_TSCEnable:
    mov rax, cr4
    or rax, 4 ; bit 2
    mov cr4, rax
    ret
