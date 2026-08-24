/* pushfq implicitly read-modify-writes %rsp (and reads RFLAGS); INPUT pushf
   has no register operands, so the unknown-mnemonic fallback modeled ZERO
   register effects: rejected (implicit register effects cannot be modeled).
   (The transform's OWN flag brackets emit pushfq/popfq at the render stage,
   which is post-validation and unaffected by this rejection.) */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	pushfq
	popq	%rcx
	movq	%rcx, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
