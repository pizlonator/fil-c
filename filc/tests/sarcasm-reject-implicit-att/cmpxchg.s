/* cmpxchg implicitly read-modify-writes %rax (the implicit accumulator
   operand) and is an atomic with a memory operand — neither is modeled:
   rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	cmpxchgq	%rcx, %rdx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
