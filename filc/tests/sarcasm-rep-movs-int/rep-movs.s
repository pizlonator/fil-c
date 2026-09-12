	.intel_syntax noprefix
	.file	"rep-movs.c"
	.text
	# Intel-syntax rep movs*/stos*, including the Intel-only dword spellings
	# (movsd/stosd — AT&T uses movsl/stosl for those).
	.globl	rep_movsq
	.type	rep_movsq, @function
rep_movsq:                      ;! void(ptr, ptr, size_t)
	mov	rcx, rdx
	rep movsq
	ret
	.size	rep_movsq, .-rep_movsq
	.globl	rep_movsd
	.type	rep_movsd, @function
rep_movsd:                      ;! void(ptr, ptr, size_t)
	mov	rcx, rdx
	rep movsd
	ret
	.size	rep_movsd, .-rep_movsd
	.globl	rep_stosd
	.type	rep_stosd, @function
rep_stosd:                      ;! void(ptr, size_t, long)
	mov	rcx, rsi
	mov	rax, rdx
	rep stosd
	ret
	.size	rep_stosd, .-rep_stosd
	.section	.note.GNU-stack,"",@progbits
