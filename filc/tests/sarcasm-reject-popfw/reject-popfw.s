# The 16-bit popfw alone is rejected the same way (see reject-pushfw.s).
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	movq	%rdi, %rax
	popfw
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
