	.text
	.globl	sum
	.type	sum, %function
sum:                            ;! long(ptr, long)
	mov	x5, #0
1:	cbz	x1, 2f
	ldr	x2, [x0], #8    ;! load ptr
	ldr	x2, [x2]
	add	x5, x5, x2
	sub	x1, x1, #1
	b	1b
2:	mov	x0, x5
	ret
	.size	sum, .-sum
	.globl	fill
	.type	fill, %function
fill:                           ;! void(ptr, ptr, long)
1:	cbz	x2, 2f
	str	x1, [x0], #8    ;! store ptr
	sub	x2, x2, #1
	b	1b
2:	ret
	.size	fill, .-fill
	.globl	sumpre
	.type	sumpre, %function
sumpre:                         ;! long(ptr, long)
	mov	x5, #0
1:	cbz	x1, 2f
	ldr	x2, [x0, #8]!   ;! load ptr
	ldr	x2, [x2]
	add	x5, x5, x2
	sub	x1, x1, #1
	b	1b
2:	mov	x0, x5
	ret
	.size	sumpre, .-sumpre
	.globl	fillpre
	.type	fillpre, %function
fillpre:                        ;! void(ptr, ptr, long)
1:	cbz	x2, 2f
	str	x1, [x0, #8]!   ;! store ptr
	sub	x2, x2, #1
	b	1b
2:	ret
	.size	fillpre, .-fillpre
	.section	.note.GNU-stack,"",@progbits
