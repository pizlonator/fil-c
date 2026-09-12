	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr, ptr)
	# rep movs* / rep stos* lower to checked copies/fills, but rep on any
	# other string op (scan/compare/load) has no checked lowering.
	rep scasb
	movq	%rdi, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
