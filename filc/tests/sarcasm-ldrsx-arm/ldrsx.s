	.text
	.globl	loadb
	.type	loadb, %function
loadb:                          ;! long(ptr, long)
	ldrsb	x0, [x0, x1]
	ret
	.size	loadb, .-loadb
	.globl	loadh
	.type	loadh, %function
loadh:                          ;! long(ptr, long)
	ldrsh	x0, [x0, x1, lsl #1]
	ret
	.size	loadh, .-loadh
	.globl	loadw
	.type	loadw, %function
loadw:                          ;! long(ptr, long)
	ldrsw	x0, [x0, x1, lsl #2]
	ret
	.size	loadw, .-loadw
	.globl	loadu
	.type	loadu, %function
loadu:                          ;! long(ptr, int)
	ldr	w0, [x0, w1, uxtw #2]
	ret
	.size	loadu, .-loadu
	.globl	loadx
	.type	loadx, %function
loadx:                          ;! long(ptr, int)
	ldr	x0, [x0, w1, uxtw #3]
	ret
	.size	loadx, .-loadx
	.section	.note.GNU-stack,"",@progbits
