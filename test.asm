global _start

section .data
    __str_0_size: dq 21
    __str_0: db 'Hello!, sweet world', 0xa, '', 0x0

section .text

; libasm
%include "asm/globals.asm"
%include "asm/lib.asm"

; fn main()->i64 ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 112
    ; rbp-8 = ret
    ; setting rbp-8 to debug value
    mov rax, 0xdeadbeef
    mov qword [rbp-8], rax
    ; rbp-16 = i64 str
    ; ret = 0
    mov qword [rbp-8], 0
    ; str = __str_0
    mov qword [rbp-16], __str_0
    ; setup arguments to std_syscall()
    ; std_syscall() arg 0 is rbp-40
    mov qword [rbp-40], 1
    ; std_syscall() arg 1 is rbp-56
    mov qword [rbp-56], 0
    ; std_syscall() arg 2 is rbp-72
    ; rbp-16 -> rax -> rbp-72
    push rax
    mov rax, qword [rbp-16]
    mov qword [rbp-72], rax
    pop rax
    ; setup arguments to deref()
    ; rbx = rbp-16 - 8
    mov rax, qword [rbp-16]
    sub rax, 8
    mov rbx, rax
    ; deref() arg 0 is rbp-104
    mov qword [rbp-104], rbx
    mov rdi, qword [rbp-104]
    ; call to deref()
    call deref
    ; std_syscall() arg 3 is rbp-88
    mov qword [rbp-88], rax
    mov rdi, qword [rbp-40]
    mov rsi, qword [rbp-56]
    mov rdx, qword [rbp-72]
    mov rcx, qword [rbp-88]
    ; call to std_syscall()
    call std_syscall
    mov rax, qword [rbp-8]
    leave
    ; return from main
    ret

; core language _start
%include "asm/_start.asm"

