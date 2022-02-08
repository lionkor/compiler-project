; fn addr(any* rdi) -> any rax
deref:
    mov rax, [rdi]
    ret

; fn ref(any rdi) -> any* rax
ref:
    lea rax, [rdi]
    ret
