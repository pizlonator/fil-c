/* xchg read-modify-writes BOTH operands, but the model classifies only a
   first def + remaining uses: the second operand's def is silently dropped.
   Rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	xchgq	%rax, %rcx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
