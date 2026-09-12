	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr, ptr)
	# bare movs*/stos* lower to checked single-step copies/fills, but the
	# other bare string ops (scan/compare/load) have no checked lowering.
	lodsq
	movq	%rdi, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
