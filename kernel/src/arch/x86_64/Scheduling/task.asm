; Copyright (©) 2022-2024  Frosty515
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

global x86_64_kernel_save_main
x86_64_kernel_save_main:
    cli ; disable interrupts

    push rax ; prepare rax to be saved later
    mov rax, [rsp+16]

    mov QWORD [rax+8], rbx   ; save rbx
    mov QWORD [rax+16], rcx  ; save rcx
    mov QWORD [rax+24], rdx  ; save rdx
    mov QWORD [rax+32], rsi  ; save rsi
    mov QWORD [rax+40], rdi  ; save rdi
    mov QWORD [rax+56], rbp  ; save rbp
    mov QWORD [rax+64],  r8  ; save r8
    mov QWORD [rax+72],  r9  ; save r9
    mov QWORD [rax+80], r10  ; save r10
    mov QWORD [rax+88], r11  ; save r11
    mov QWORD [rax+96], r12  ; save r12
    mov QWORD [rax+104], r13 ; save r13
    mov QWORD [rax+112], r14 ; save r14
    mov QWORD [rax+120], r15 ; save r15

    pop rbx
    mov QWORD [rax], rbx ; save rax

    pop rbx
    mov QWORD [rax+128], rbx ; save RIP

    pop rbx ; get rid of saving address

    mov rbx, QWORD [rax+128]
    push rbx ; place return address back on stack

    mov QWORD [rax+48], rsp ; finally can save rsp properly

    xor rbx, rbx
    mov bx, cs
    mov WORD [rax+136], bx ; save cs

    xor rbx, rbx
    mov bx, ds
    mov WORD [rax+138], bx ; save ds

    pushf
    pop rbx
    mov QWORD [rax+140], rbx ; save rflags

    mov rbx, cr3
    mov QWORD [rax+148], rbx ; save cr3

    mov rbx, QWORD [rax+8] ; restore rbx
    mov rax, QWORD [rax]   ; restore rax

    sti ; enable interrupts
    ret

global x86_64_get_stack_ptr
x86_64_get_stack_ptr:
    mov rax, rsp
    ret

extern kernel_stack
extern kernel_stack_size

global x86_64_kernel_thread_end
x86_64_kernel_thread_end:
    cli
    pop rax
    mov rsp, kernel_stack
    add rsp, [kernel_stack_size]
    xor rbp, rbp
    push rax
    ret

global x86_64_context_switch
x86_64_context_switch:
    pushf
    cli ; disable interrupts

    mov rbx, QWORD [rdi+8]   ; load rbx
    mov rdx, QWORD [rdi+24]  ; load rdx
    mov rsi, QWORD [rdi+32]  ; load rsi
    mov rbp, QWORD [rdi+56]  ; load rbp
    mov  r8, QWORD [rdi+64]  ; load r8
    mov  r9, QWORD [rdi+72]  ; load r9
    mov r10, QWORD [rdi+80]  ; load r10
    mov r11, QWORD [rdi+88]  ; load r11
    mov r12, QWORD [rdi+96]  ; load r12
    mov r13, QWORD [rdi+104] ; load r13
    mov r14, QWORD [rdi+112] ; load r14
    mov r15, QWORD [rdi+120] ; load r15

    mov ax, WORD [rdi+138]
    mov ds, ax ; load ds
    mov es, ax ; load es
    mov fs, ax ; load fs
    mov gs, ax ; load gs

    mov rax, QWORD [rdi+148]
    mov cr3, rax ; load CR3

    pop rcx ; get RFLAGS back

    movzx rax, WORD [rdi+138] ; push ss
    push rax

    mov rax, QWORD [rdi+48] ; push rsp
    push rax
    
    push rcx ; push rflags

    movzx rax, WORD [rdi+136] ; push cs
    push rax

    mov rax, QWORD [rdi+128] ; push RIP
    push rax

    mov rax, QWORD [rdi] ; load rax
    mov rcx, QWORD [rdi+16] ; load rcx
    mov rdi, QWORD [rdi+40] ; load rdi
    iretq

global x86_64_set_kernel_gs_base
x86_64_set_kernel_gs_base:
    xor rax, rax
    xor rdx, rdx
    mov eax, edi
    shr rdi, 32
    mov edx, edi
    mov rcx, 0xc0000102
    wrmsr
    ret

global x86_64_get_kernel_gs_base:
x86_64_get_kernel_gs_base:
    xor rdx, rdx
    xor rax, rax
    mov rcx, 0xc0000102
    rdmsr
    shl rdx, 32
    or rax, rdx
    ret

global x86_64_idle_loop
x86_64_idle_loop:
    sti
.l:
    jmp .l
