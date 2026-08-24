global gdt_flush
gdt_flush:
    lgdt [rdi]            ; RDI contiene el primer argumento (la dirección de gdtr)
    
    ; Actualiza los selectores de datos (0x10 es el offset del Kernel Data: 2do descriptor)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump para actualizar CS (0x08 es el offset del Kernel Code: 1er descriptor)
    push 0x08
    lea rax, [.reload_cs]
    push rax
    retfq                 ; Far return en 64 bits para emular el far jump

.reload_cs:
    ret
