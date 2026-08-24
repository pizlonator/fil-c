	.text
	.globl	popcount
	.type	popcount, %function
popcount:                       ;! long(long)
	mov	x1, #0
1:	cbz	x0, 2f
	tbz	x0, #0, 3f
	add	x1, x1, #1
3:	lsr	x0, x0, #1
	b	1b
2:	mov	x0, x1
	ret
	.size	popcount, .-popcount
	.globl	tbnzloop
	.type	tbnzloop, %function
tbnzloop:                       ;! long(long)
	mov	x1, #0
1:	tbnz	x0, #63, 2f
	add	x0, x0, x0
	add	x1, x1, #1
	b	1b
2:	mov	x0, x1
	ret
	.size	tbnzloop, .-tbnzloop
	.globl	max2
	.type	max2, %function
max2:                           ;! long(long, long)
	cmp	x0, x1
	csel	x0, x0, x1, ge
	ret
	.size	max2, .-max2
	.globl	boolize
	.type	boolize, %function
boolize:                        ;! long(long)
	cmp	x0, #0
	cset	x0, ne
	ret
	.size	boolize, .-boolize
	.globl	movnval
	.type	movnval, %function
movnval:                        ;! long()
	movn	x0, #0
	ret
	.size	movnval, .-movnval
	.globl	litval
	.type	litval, %function
litval:                         ;! long()
	ldr	x0, =0x123456789abcdef0
	ret
	.size	litval, .-litval
	.section	.note.GNU-stack,"",@progbits
