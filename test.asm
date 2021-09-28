global _start
section .text

; add(i64 a, i64 b) -> i64 c
; c = rax
; a = rdi
; b = rsi
fn_add:
    ; c = a + b
    ; rax = rdi + rsi
    ; push rdi; rdi += rsi; rax = rdi; pop rdi
    push rdi
    add rdi, rsi
    mov rax, rdi
    pop rdi
    ret

fn_main:
    ; ret = add(1, 2)
    ; rax = add(rdi, rsi)
    ; rdi = 1; rsi = 2; rax = add(rdi, rsi)
    mov rdi, 1
    mov rsi, 2
    call fn_add
    ret

; implementation specific
_start:
    call fn_main
    mov rdi, rax
    mov rax, 60
    syscall

