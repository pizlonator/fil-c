	.text
	.globl	misload
	.type	misload, %function
misload:                        ;! ptr(ptr)
	ldr	x0, [x0]        ;! load ptr
	ret
	.size	misload, .-misload
	.section	.note.GNU-stack,"",@progbits
