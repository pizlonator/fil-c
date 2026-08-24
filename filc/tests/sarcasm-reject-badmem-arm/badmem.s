/* A memory operand on an instruction that is not a load or a store cannot be
   given a bounds check, so it is rejected rather than rewritten. */
	.arch armv8-a
	.text
	.global	foo
	.type	foo, %function
foo:                            ;! uint64_t(ptr)
	mov	x1, [x0]
	ret
	.size	foo, .-foo
