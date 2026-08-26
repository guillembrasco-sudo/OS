[bits 32]

%define KERNEL_VMA 0xffffffff80000000

extern _kernel_phys_load_end
extern _kernel_phys_bss_end

section .multiboot
align 4
multiboot_header:
    dd 0x1BADB002                               ; Magic Number para Multiboot 1
    dd (1 << 0) | (1 << 1) | (1 << 2) | (1 << 16) ; Align + MemInfo + Video + AOUT Kludge
    dd - (0x1BADB002 + ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 16))) ; Checksum

    ; Campos obligatorios AOUT Kludge (Bit 16):
    dd multiboot_header - KERNEL_VMA            ; header_addr
    dd 0x00100000                               ; load_addr
    dd _kernel_phys_load_end                    ; load_end_addr (Dirección física real)
    dd _kernel_phys_bss_end                     ; bss_end_addr  (Dirección física real)
    dd start - KERNEL_VMA                       ; entry_addr
    dd 0                                         ; mode_type: linear graphics
    dd 1024                                      ; requested width
    dd 768                                       ; requested height
    dd 32                                        ; requested depth
multiboot_header_end:

section .text
global start
global boot_kaslr_seed
global stack_top
extern kmain

start:
    cli

    ; 1. Preservar argumentos de Multiboot ANTES de ejecutarse rdtsc
    mov edi, eax                                ; RDI/EDI = 1er argumento (Multiboot Magic: 0x2BADB002)
    mov esi, ebx                                ; RSI/ESI = 2do argumento (Puntero físico a multiboot_info)

    push edi
    push esi
    mov edi, page_table_l4 - KERNEL_VMA
    xor eax, eax
    mov ecx, (4096 * 3) / 4
    rep stosd
    pop esi
    pop edi

    ; 2. Generar semilla KASLR sin perder EAX/EBX
    rdtsc
    mov [boot_kaslr_seed - KERNEL_VMA], eax
    mov [boot_kaslr_seed - KERNEL_VMA + 4], edx

    ; Asignar stack temporal mediante dirección física
    mov esp, stack_top - KERNEL_VMA

    ; Cargar GDT en dirección física
    lgdt [gdt64.pointer - KERNEL_VMA]

    ; Configurar PML4: Índice 0 (Identity) e Índice 511 (Higher-Half)
    mov eax, page_table_l4 - KERNEL_VMA
    mov edx, page_table_pdpt - KERNEL_VMA
    or edx, 0x03
    mov dword [eax], edx         ; PML4[0] lower 32 bits
    mov dword [eax + 4], 0       ; PML4[0] upper 32 bits

    mov dword [eax + 511 * 8], edx     ; PML4[511] lower 32 bits
    mov dword [eax + 511 * 8 + 4], 0 ; PML4[511] upper 32 bits

    ; Configurar PDPT: Índice 0 (Identity) e Índice 510 (Higher-Half)
    mov eax, page_table_pdpt - KERNEL_VMA
    mov edx, page_table_pd - KERNEL_VMA
    or edx, 0x03                                ; Present + Writable
    mov dword [eax], edx            ; PDPT[0] lower 32 bits
    mov dword [eax + 4], 0          ; PDPT[0] upper 32 bits (FIXED)
    mov dword [eax + 510 * 8], edx  ; PDPT[510] lower 32 bits
    mov dword [eax + 510 * 8 + 4], 0; PDPT[510] upper 32 bits (FIXED)

    ; Mapear múltiples páginas de 2 MB en la Page Directory (PD)
    mov eax, page_table_pd - KERNEL_VMA
    mov ebx, 0x00000083                         ; Present + Writable + Huge Page (PS=1)
    mov ecx, 0
.map_pd_loop:
    mov [eax + ecx * 8], ebx
    mov dword [eax + ecx * 8 + 4], 0
    add ebx, 0x00200000
    inc ecx
    cmp ecx, 512 ; Maps 1 GB (512 * 2 MB)
    jl .map_pd_loop

    ; Habilitar PAE
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10) ; Bit 5 = PAE
    mov cr4, eax

    ; Cargar CR3
    mov eax, page_table_l4 - KERNEL_VMA
    and eax, 0xFFFFF000          ; Ensure 4096-byte boundary
    mov cr3, eax

    ; Habilitar Long Mode en MSR IA32_EFER
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Activar Paginación y Modo Protegido
    mov eax, cr0
    or eax, (1 << 31) | 1
    mov cr0, eax

    ; Salto lejano a código de 64 bits
    jmp 0x08:(long_mode_start - KERNEL_VMA)

[bits 64]
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rax, higher_half
    jmp rax

higher_half:
    mov rsp, stack_top
    xor rbp, rbp

    ; rdi (Magic) y rsi (multiboot_info) se mantienen intactos desde start
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
    dq 0x00cf92000000ffff                      ; Kernel Data 64-bit (DS = 0x10)
.pointer:
    dw $ - gdt64 - 1
    dd gdt64 - KERNEL_VMA

section .bss
align 4096
page_table_l4: resq 512
page_table_pdpt: resq 512
page_table_pd: resq 512
stack_bottom: resb 16384
stack_top:
boot_kaslr_seed: resq 1