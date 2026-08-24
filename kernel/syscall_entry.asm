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

    push qword [gs:0]           ; guarda user RSP en la stack del kernel para poder volver
    ; Convención: RAX = número de syscall, RDI/RSI/RDX/R10/R8 = args
    ; (R10 en vez de RCX porque SYSCALL destruye RCX con la dirección de
    ; retorno).
    mov rcx, r10                ; reordena para calzar con el 4º argumento de syscall_dispatch en la ABI
    call syscall_dispatch       ; rax ya contiene el número, pero syscall_dispatch lo espera como 1er arg
                                 ; -> en la práctica: mueve rax a rdi ANTES del call (ver nota abajo)

    pop rsp                     ; restaura user RSP
    swapgs
    o64 sysret