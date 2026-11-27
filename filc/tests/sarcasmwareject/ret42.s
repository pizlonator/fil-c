	.text
	.globl	ret42
	.type	ret42, @function
ret42:                          ;! long()
	movq	$42, %rax
	ret
	.size	ret42, .-ret42
	.section	.note.GNU-stack,"",@progbits
