/* sahf implicitly USES %rax (AH -> SF/ZF/AF/PF/CF), but it has no register
   operands, so the unknown-mnemonic fallback modeled ZERO register effects:
   rejected like lahf (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	sahf
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
