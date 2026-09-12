	.file	"rep-stos.c"
	.text
	# rep stos* in every element width, plus the bare single-step form.
	.globl	rep_stosq
	.type	rep_stosq, @function
rep_stosq:                      ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosq
	ret
	.size	rep_stosq, .-rep_stosq
	.globl	rep_stosb
	.type	rep_stosb, @function
rep_stosb:                      ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosb
	ret
	.size	rep_stosb, .-rep_stosb
	.globl	rep_stosw
	.type	rep_stosw, @function
rep_stosw:                      ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosw
	ret
	.size	rep_stosw, .-rep_stosw
	.globl	rep_stosl
	.type	rep_stosl, @function
rep_stosl:                      ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosl
	ret
	.size	rep_stosl, .-rep_stosl
	.globl	bare_stosq
	.type	bare_stosq, @function
bare_stosq:                     ;! void(ptr, long)
	movq	%rsi, %rax
	stosq
	ret
	.size	bare_stosq, .-bare_stosq
	.section	.note.GNU-stack,"",@progbits
