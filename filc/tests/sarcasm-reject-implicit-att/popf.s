/* popfq implicitly read-modify-writes %rsp (and writes RFLAGS); INPUT popf
   has no register operands, so the unknown-mnemonic fallback modeled ZERO
   register effects: rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	pushq	%rcx
	popfq
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
