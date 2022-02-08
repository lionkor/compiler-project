global _start

section .data
	; externs from dependency "std/print.o"
	extern std_print
	; own globals
	global main
	; own data
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
    sub rsp, 56
    ; rbp-8 = ret
    ; setting rbp-8 to debug value
    mov rax, 0xdeadc0de
    mov qword [rbp-8], rax
    ; ret = 0
    mov qword [rbp-8], 0
    ; rbp-24 = i64 str
    ; str = __str_0
    mov qword [rbp-24], __str_0
    ; setup arguments to std_print()
    ; std_print() arg 0 is rbp-48
    ; rbp-24 -> rax -> rbp-48
    push rax
    mov rax, qword [rbp-24]
    mov qword [rbp-48], rax
    pop rax
    mov rdi, qword [rbp-48]
    ; call to std_print()
    call std_print
    ; ret = rax
    mov qword [rbp-8], rax
    mov rax, qword [rbp-8]
    leave
    ; return from main
    ret

; core language _start
%include "asm/_start.asm"

