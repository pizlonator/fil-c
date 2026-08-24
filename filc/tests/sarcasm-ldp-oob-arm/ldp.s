	.text
	.globl	loadpair
	.type	loadpair, %function
loadpair:                       ;! long(ptr)
	ldp	x2, x3, [x0]
	add	x0, x2, x3
	ret
	.size	loadpair, .-loadpair
	.section	.note.GNU-stack,"",@progbits
