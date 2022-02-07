; fn addr(any* rdi) -> any rax
deref:
    mov rax, [rdi]
    ret

; fn addr(any rdi) -> any* rax
addr:
    lea rax, [rdi]
    ret
