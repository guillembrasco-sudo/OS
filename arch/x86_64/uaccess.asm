bits 64
global copy_from_user_asm
global copy_to_user_asm

copy_from_user_asm:
    cld
    rep movsb
    xor eax, eax
    ret

copy_to_user_asm:
    cld
    rep movsb
    xor eax, eax
    ret

section __ex_table
    dq copy_from_user_asm, copy_from_user_fault
    dq copy_to_user_asm, copy_to_user_fault

section .text
copy_from_user_fault:
    mov eax, -1
    ret

copy_to_user_fault:
    mov eax, -1
    ret