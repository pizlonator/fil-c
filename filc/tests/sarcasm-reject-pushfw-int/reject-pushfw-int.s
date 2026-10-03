# The 16-bit pushfw is rejected in Intel-syntax input too.
	.intel_syntax noprefix
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	mov	rax, rdi
	pushfw
	popfw
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
