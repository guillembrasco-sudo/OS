[bits 16]

%define TRAMPOLINE_PHYS 0x8000
%define CONFIG_CR3      0x7000
%define CONFIG_STACK    0x7008
%define CONFIG_ENTRY    0x7010
%define CONFIG_APIC_ID  0x7018

section .text
align 16
global trampoline_start
global trampoline_end

trampoline_start:
    cli
    xor ax, ax
    mov ds, ax
    lgdt [TRAMPOLINE_PHYS + trampoline_gdtr - trampoline_start]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:(TRAMPOLINE_PHYS + trampoline_protected - trampoline_start)

[bits 32]
trampoline_protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, [CONFIG_CR3]
    mov cr3, eax
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax
    mov ecx, 0xc0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax
    jmp 0x18:(TRAMPOLINE_PHYS + trampoline_long - trampoline_start)

[bits 64]
trampoline_long:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, [CONFIG_STACK]
    mov edi, [CONFIG_APIC_ID]
    mov rax, [CONFIG_ENTRY]
    call rax
.halt:
    cli
    hlt
    jmp .halt

align 8
trampoline_gdt:
    dq 0
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff
trampoline_gdtr:
    dw trampoline_gdtr - trampoline_gdt - 1
    dd TRAMPOLINE_PHYS + trampoline_gdt - trampoline_start

trampoline_end:
