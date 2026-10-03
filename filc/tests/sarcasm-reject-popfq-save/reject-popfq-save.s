# A popfq whose top-of-stack word is a DROPPED register save: a callee-saved
# push's save slot is never emitted (the dropped-save model keeps the value
# only in the register's web), so the popfq has no modeled word to load.
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	pushq	%rbx
	movq	%rdi, %rbx
	popfq
	movq	%rbx, %rax
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
