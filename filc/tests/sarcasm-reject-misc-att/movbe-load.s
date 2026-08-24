/* movbe is a byte-swapping load/store: the model does not know its memory
   access shape or its swap semantics, so it cannot be bounds-checked or
   modeled: rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr)
	movbeq	(%rdi), %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
