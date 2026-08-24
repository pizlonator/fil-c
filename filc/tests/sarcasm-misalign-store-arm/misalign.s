	.text
	.globl	misstore
	.type	misstore, %function
misstore:                       ;! void(ptr, ptr)
	str	x1, [x0]        ;! store ptr
	ret
	.size	misstore, .-misstore
	.section	.note.GNU-stack,"",@progbits
