global x86_enter_user_mode

; void x86_enter_user_mode(uint64_t entry, uint64_t user_stack)
; GDT selectors with RPL=3: user code 0x1b, user data 0x23.
x86_enter_user_mode:
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push 0x23
    push rsi
    pushfq
    or qword [rsp], 0x200
    push 0x1b
    push rdi
    iretq