	.text
	.globl	wrapper
	.type	wrapper, @function
wrapper:                        ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	call	maythrow        ;! long(long)
	addq	$1, %rax
	popq	%rbp
	ret
	.size	wrapper, .-wrapper
	.section	.note.GNU-stack,"",@progbits
