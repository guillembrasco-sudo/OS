; kernel/syscall_entry.asm — entrada vía SYSCALL (requiere habilitar
; EFER.SCE y programar STAR/LSTAR/SFMASK en el MSR durante syscall_init,
; código de configuración de MSR omitido aquí por brevedad pero es
; mecánico: wrmsr con IA32_LSTAR = syscall_entry, IA32_STAR con los
; selectores de CS/SS kernel y usuario, IA32_FMASK para limpiar IF).
global syscall_entry
extern syscall_dispatch
extern tss_set_kernel_stack

syscall_entry:
    swapgs                      ; cambia a GS del kernel (para acceder a per-CPU data)
    ; SYSCALL no cambia de stack automáticamente como iret: hay que
    ; conmutar manualmente a la stack del kernel guardando RSP de usuario.
    mov [gs:0], rsp             ; asume slot per-CPU en offset 0 para guardar user RSP
    mov rsp, [gs:8]             ; asume slot per-CPU en offset 8 con el kernel RSP (rsp0)

    ; SYSCALL entrega RIP en RCX y RFLAGS en R11; ambos deben sobrevivir al
    ; call. Se guardan junto con el RSP de usuario en la pila del kernel.
    push r11
    push rcx
    push qword [gs:0]
    sub rsp, 8                   ; mantiene alineacion SysV antes del call

    ; Convencion de entrada: RAX=num, RDI/RSI/RDX/R10/R8=args.
    ; syscall_dispatch usa la ABI C: RDI=num, RSI=a1, RDX=a2,
    ; RCX=a3, R8=a4, R9=a5.
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call syscall_dispatch

    add rsp, 8
    pop rdx                       ; user RSP
    pop rcx                       ; user RIP
    pop r11                       ; user RFLAGS
    mov rsp, rdx
    swapgs
    o64 sysret