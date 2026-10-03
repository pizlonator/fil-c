# A pushfq with no matching popfq leaks an 8-byte word: the return would run
# with a perturbed stack pointer (the real rsp is 8 lower than the model's),
# so the depth analysis rejects it exactly like a leaked register push.
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	movq	%rdi, %rax
	pushfq
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
