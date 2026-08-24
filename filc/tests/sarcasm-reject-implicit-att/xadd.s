/* xadd exchanges and adds: BOTH operands are read-modify-write (and it is an
   atomic with a memory operand), but the model would classify only a first
   def + remaining uses: rejected (implicit register effects cannot be
   modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	xaddq	%rax, %rcx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
