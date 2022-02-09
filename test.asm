global _start

section .data
	; own globals
	global main
	; own data
    __str_0_size: dq 1
    __str_0: db 'a', 0x0

section .text

; libasm
%include "asm/lib.asm"

; fn main()->i64 ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    ; rbp-8 = ret
    ; setting rbp-8 to debug value
    mov rax, 0xdeadc0de
    mov qword [rbp-8], rax
    ; rbp-16 = u64 s
    ; s = __str_0
    mov qword [rbp-16], __str_0
    ; setup arguments to deref8()
    ; deref8() arg 0 is rbp-40
    ; rbp-16 -> rax -> rbp-40
    push rax
    mov rax, qword [rbp-16]
    mov qword [rbp-40], rax
    pop rax
    mov rdi, qword [rbp-40]
    ; call to deref8()
    call deref8
    ; ret = rax
    mov qword [rbp-8], rax
    mov rax, qword [rbp-8]
    leave
    ; return from main
    ret

; core language _start
%include "asm/_start.asm"

