[bits 32]

%define KERNEL_VMA 0xffffffff80000000

section .multiboot
align 4
multiboot_header:
    dd 0x1BADB002                              ; Magic Number para Multiboot 1
    dd (1 << 0) | (1 << 1) | (1 << 16)         ; Flags: Align + MemInfo + AOUT Kludge (Bit 16)
    dd - (0x1BADB002 + ((1 << 0) | (1 << 1) | (1 << 16))) ; Checksum

    ; Campos obligatorios al activar el AOUT Kludge (Bit 16):
    dd multiboot_header - KERNEL_VMA           ; header_addr: dirección física de esta cabecera
    dd 0x00100000                              ; load_addr: dirección física de inicio en RAM
    dd 0                                       ; load_end_addr
    dd 0                                       ; bss_end_addr
    dd start - KERNEL_VMA                      ; entry_addr: dirección física de 'start'
multiboot_header_end:

section .text
global start
global boot_kaslr_seed
extern kmain

start:
    cli
    rdtsc
    mov [boot_kaslr_seed - KERNEL_VMA], eax
    mov [boot_kaslr_seed - KERNEL_VMA + 4], edx

    ; Asignar stack temporal mediante dirección física
    mov esp, stack_top - KERNEL_VMA

    ; Cargar GDT en dirección física
    lgdt [gdt64.pointer - KERNEL_VMA]

    ; 1. Configurar PML4: Índice 0 (Identity) e Índice 511 (Higher-Half)
    mov eax, page_table_l4 - KERNEL_VMA
    mov edx, page_table_pdpt - KERNEL_VMA
    or edx, 0x03                               ; Present + Writable
    mov [eax], edx                             ; PML4[0] (Identidad durante transición)
    mov [eax + 511 * 8], edx                   ; PML4[511] (Dirección base 0xFFFFFFFF80000000)

    ; 2. Configurar PDPT: Índice 0 (Identity) e Índice 510 (Higher-Half)
    mov eax, page_table_pdpt - KERNEL_VMA
    mov edx, page_table_pd - KERNEL_VMA
    or edx, 0x03                               ; Present + Writable
    mov [eax], edx                             ; PDPT[0]
    mov [eax + 510 * 8], edx                   ; PDPT[510]

    ; 3. Mapear múltiples páginas de 2 MB en la Page Directory (PD)
    mov eax, page_table_pd - KERNEL_VMA
    mov ebx, 0x00000083                        ; Present + Writable + Huge Page (Bit PS=1)
    mov ecx, 0
.map_pd_loop:
    mov [eax + ecx * 8], ebx
    mov dword [eax + ecx * 8 + 4], 0           ; Limpiar los 32 bits superiores del PCDE
    add ebx, 0x00200000                        ; Avanzar 2 MB en la dirección física
    inc ecx
    cmp ecx, 8                                 ; 8 entradas = 16 MB mapeados
    jl .map_pd_loop

    ; Habilitar PAE (Physical Address Extension) en CR4
    mov eax, cr4
    or eax, (1 << 5)                           ; Bit 5: PAE
    or eax, (1 << 9) | (1 << 10)               ; Bit 9: OSFXSR, Bit 10: OSXMMEXCPT (SSE)
    mov cr4, eax

    ; Cargar CR3 con la dirección física del PML4
    mov eax, page_table_l4 - KERNEL_VMA
    mov cr3, eax

    ; Habilitar Long Mode en MSR IA32_EFER (Bit 8: LME)
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Activar Paginación (Bit 31) y Modo Protegido (Bit 0) en CR0
    mov eax, cr0
    or eax, 1 << 31
    or eax, 1
    mov cr0, eax

    ; Salto lejano a código de 64 bits (en memoria física baja)
    jmp 0x08:(long_mode_start - KERNEL_VMA)

[bits 64]
long_mode_start:
    ; Actualizar selectores de datos de 64 bits
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Transición explícita al espacio de memoria virtual superior (Higher-Half)
    mov rax, higher_half
    jmp rax

higher_half:
    ; Cargar RSP con la dirección VMA absoluta de 64 bits
    mov rsp, stack_top
    xor rbp, rbp

    ; Invocar el punto de entrada principal del Kernel
    call kmain

.halt:
    cli
    hlt
    jmp .halt

section .rodata
align 8
gdt64:
    dq 0
    dq 0x00af9a000000ffff                      ; Kernel Code 64-bit (CS = 0x08)
    dq 0x00af92000000ffff                      ; Kernel Data 64-bit (DS = 0x10)
.pointer:
    dw $ - gdt64 - 1
    dd gdt64 - KERNEL_VMA                      ; 32-bit pointer para lgdt en 32-bit mode

section .bss
align 4096
page_table_l4: resq 512
page_table_pdpt: resq 512
page_table_pd: resq 512
stack_bottom: resb 16384
stack_top:
boot_kaslr_seed: resq 1