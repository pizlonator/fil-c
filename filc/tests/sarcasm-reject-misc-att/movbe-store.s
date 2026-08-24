/* movbe store form: an unmodeled memory access plus a byte swap: rejected
   like the load form (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr, long)
	movq	%rsi, %rax
	movbeq	%rax, (%rdi)
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
