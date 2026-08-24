/* A signature-shaped annotation only makes sense on a call: on any other
   instruction it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	add	x0, x0, x1          ;! int(ptr)
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

