global deref
global deref8
global ref

; fn addr(any* rdi) -> any rax
deref:
    mov rax, [rdi]
    ret

deref8:
    push rbp
    mov rbp, rsp
    mov qword [rbp-8], rdi
    mov rax, qword [rbp-8]
    movzx eax, byte [rax]
    pop rbp
    ret

; fn ref(any rdi) -> any* rax
ref:
    lea rax, [rdi]
    ret
