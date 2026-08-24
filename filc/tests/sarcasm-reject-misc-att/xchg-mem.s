/* xchg with a memory operand is an IMPLICIT-LOCK atomic read-modify-write on
   memory plus a both-operands register RMW — neither is modeled: rejected
   (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr)
	movq	$1, %rax
	xchgq	%rax, (%rdi)
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
