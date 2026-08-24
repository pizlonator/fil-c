	.arch armv8-a
	.text
	.globl	mk
	.type	mk, %function
mk:                             ;! unsigned long(ptr)
	ldr	x0, [x0]
	mov	x1, 43175
	movk	x1, 0xbc5, lsl 16
	add	x0, x0, x1
	ret
	.size	mk, .-mk
