# A popfq with nothing pushed: the word it would load has no provable
# materialized value (the stack model has no word at this depth), so it is
# rejected instead of loading garbage into RFLAGS.
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	movq	%rdi, %rax
	popfq
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
