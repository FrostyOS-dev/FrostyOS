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

global x86_64_outb
global x86_64_inb
global x86_64_outw
global x86_64_inw

x86_64_outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret

x86_64_inb:
    mov dx, di
    in al, dx
    ret

x86_64_outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

x86_64_inw:
    mov dx, di
    in ax, dx
    ret
