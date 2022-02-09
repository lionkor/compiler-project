global _start

section .data
	; externs from dependency "std/print.o"
	extern std_print
	; own globals
	global main
	; own data
    __str_0_size: dq 1
    __str_0: db 'A', 0x0
    __str_1_size: dq 1
    __str_1: db 'B', 0x0
    __str_2_size: dq 1
    __str_2: db 'C', 0x0

section .text

; libasm
%include "asm/lib.asm"

; fn main()->i64 ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    ; rbp-8 = ret
    ; setting rbp-8 to debug value
    mov rax, 0xdeadc0de
    mov qword [rbp-8], rax
    ; rbp-16 = i64 str
    ; condition of if-statement
    push rax
    mov rax, 0
    cmp rax, 0
    pop rax
    ; jump to else/end
    je ___0
    ; if body
    ; str = __str_0
    mov qword [rbp-16], __str_0
    ; jump to end, past the else
    jmp ___1
___0:
    ; else body
    ; condition of if-statement
    push rax
    mov rax, 1
    cmp rax, 0
    pop rax
    ; jump to else/end
    je ___2
    ; if body
    ; str = __str_1
    mov qword [rbp-16], __str_1
    ; jump to end, past the else
    jmp ___3
___2:
    ; else body
    ; str = __str_2
    mov qword [rbp-16], __str_2
___3:
___1:
    ; setup arguments to std_print()
    ; std_print() arg 0 is rbp-72
    ; rbp-16 -> rax -> rbp-72
    push rax
    mov rax, qword [rbp-16]
    mov qword [rbp-72], rax
    pop rax
    mov rdi, qword [rbp-72]
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

