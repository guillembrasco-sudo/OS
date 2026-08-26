; window_ops.asm
; x86_64 NASM, System V AMD64 ABI
; ------------------------------------------------------------
; Exported:
;   asm_fast_clear_buffer(void *buffer, uint32_t pixel_count, uint32_t color)
;   asm_window_hit_test(int32_t px, int32_t py,
;                       int32_t x, int32_t y,
;                       int32_t width, int32_t height)
;
; No callee-saved registers are modified.
; AVX2 path is selected at compile-time via WINDOW_OPS_NO_AVX2.
; The code itself does not execute CPUID. The kernel/build system must
; select this object only on CPUs with AVX2 support.
;
; SysV AMD64:
;   arg1 = RDI
;   arg2 = RSI
;   arg3 = RDX
;   arg4 = RCX
;   arg5 = R8
;   arg6 = R9
;
; YMM/XMM registers are caller-saved under SysV AMD64, so they require
; no preservation.

default rel
section .text

global asm_fast_clear_buffer
global asm_window_hit_test

; ------------------------------------------------------------
; void asm_fast_clear_buffer(void *buffer, uint32_t pixel_count,
;                            uint32_t color)
;
; Pixel order is RGBA32 as an opaque 32-bit word.
; Uses AVX2 to write 8 pixels (32 bytes) per iteration, then a scalar
; tail. vmovdqu is used deliberately: no alignment precondition is
; imposed on the framebuffer address.
; ------------------------------------------------------------

asm_fast_clear_buffer:
%ifndef WINDOW_OPS_NO_AVX2
    ; RDI = destination
    ; ESI = number of 32-bit pixels
    ; EDX = packed 32-bit color

    test    rdi, rdi
    jz      .done

    test    esi, esi
    jz      .done

    ; Broadcast the 32-bit color to all eight dwords in YMM0.
    vmovd   xmm0, edx
    vpbroadcastd ymm0, xmm0

    ; EAX = pixel count rounded down to a multiple of 8.
    mov     eax, esi
    and     eax, -8

    ; R8D = vector pixel count consumed so far.
    xor     r8d, r8d

.vector_loop:
    cmp     r8d, eax
    jae     .scalar_tail

    ; Store 8 RGBA32 pixels = 32 bytes.
    vmovdqu [rdi], ymm0
    add     rdi, 32
    add     r8d, 8
    jmp     .vector_loop

.scalar_tail:
    ; Process remaining 0..7 pixels.
    mov     ecx, esi
    sub     ecx, eax
    jz      .vzero_upper

    ; EDX still contains the packed color.
.scalar_loop:
    mov     [rdi], edx
    add     rdi, 4
    dec     ecx
    jnz     .scalar_loop

.vzero_upper:
    ; Avoid carrying a live AVX upper state into caller code.
    vzeroupper

.done:
    ret
%else
    ; Portable baseline: REP STOSD.
    ; RDI = destination
    ; ESI = dword count
    ; EDX = dword color
    test    rdi, rdi
    jz      .done_scalar

    test    esi, esi
    jz      .done_scalar

    mov     eax, edx
    mov     ecx, esi
    rep stosd

.done_scalar:
    ret
%endif

; ------------------------------------------------------------
; int asm_window_hit_test(int32_t px, int32_t py,
;                         int32_t x, int32_t y,
;                         int32_t width, int32_t height)
;
; Returns:
;   EAX = 1 when x <= px < x+width AND y <= py < y+height
;   EAX = 0 otherwise
;
; Width/height <= 0 is treated as outside.
; Uses sign-aware arithmetic by keeping operands in 32-bit registers
; and comparing transformed coordinates without modifying any
; non-volatile register.
; ------------------------------------------------------------

asm_window_hit_test:
    ; width <= 0 => false
    test    r8d, r8d
    jle     .not_inside

    ; height <= 0 => false
    test    r9d, r9d
    jle     .not_inside

    ; Check px >= x
    cmp     edi, edx
    jl      .not_inside

    ; Check py >= y
    cmp     esi, ecx
    jl      .not_inside

    ; Check px < x + width using 64-bit arithmetic to avoid signed
    ; overflow at the upper edge of the coordinate range.
    movsxd  r10, edx
    movsxd  r11, r8d
    add     r10, r11
    movsxd  rax, edi
    cmp     rax, r10
    jge     .not_inside

    ; Check py < y + height.
    movsxd  r10, ecx
    movsxd  r11, r9d
    add     r10, r11
    movsxd  rax, esi
    cmp     rax, r10
    jge     .not_inside

    mov     eax, 1
    ret

.not_inside:
    xor     eax, eax
    ret
