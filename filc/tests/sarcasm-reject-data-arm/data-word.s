/* A .word inside a function body emits constant data where the control flow can
   fall into it: rejected (data in a function body is not supported). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	.word	0xdeadbeef
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

