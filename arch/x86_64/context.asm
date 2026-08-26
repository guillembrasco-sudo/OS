[bits 64]
global context_switch

; void context_switch(struct cpu_context *old, const struct cpu_context *next)
context_switch:
	mov [rdi + 0],  rsp
	mov [rdi + 8],  rbp
	mov [rdi + 16], rbx
	mov [rdi + 24], r12
	mov [rdi + 32], r13
	mov [rdi + 40], r14
	mov [rdi + 48], r15
	mov rsp, [rsi + 0]
	mov rbp, [rsi + 8]
	mov rbx, [rsi + 16]
	mov r12, [rsi + 24]
	mov r13, [rsi + 32]
	mov r14, [rsi + 40]
	mov r15, [rsi + 48]
	ret
