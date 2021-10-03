global _start

section .data
    __str_0_size: dq 8
    __str_0: db 'Hello!', 0xa, '', 0x0

section .text

; libasm
%include "asm/globals.asm"
%include "asm/lib.asm"

; fn main()->i64 ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    ; rbp-8 = ret
    ; setting rbp-8 to debug value
    mov rax, 0xdeadbeef
    mov qword [rbp-8], rax
    ; rbp-16 = i64 str
    ; str = __str_0
    mov qword [rbp-16], __str_0
    mov rdi, 1
    mov rsi, 0
    mov rdx, qword [rbp-16]
    ; rbx = rbp-16 - 8
    mov rax, qword [rbp-16]
    sub rax, 8
    mov rbx, rax
    mov rcx, rbx
    call std_syscall
    ; ret = rax
    mov qword [rbp-8], rax
    mov rax, qword [rbp-8]
    leave
    ; return from main
    ret

; core language _start
%include "asm/_start.asm"

