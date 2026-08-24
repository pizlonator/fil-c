	.text
	.global	h
	.type	h, %function
h:                              ;! long(long, long, ptr)
	cmp	x0, x1
	ldr	x0, [x2]
	add	x1, x0, 5
	csel	x0, x1, x0, eq
	ret
	.size	h, .-h
	.section	.note.GNU-stack,"",@progbits
