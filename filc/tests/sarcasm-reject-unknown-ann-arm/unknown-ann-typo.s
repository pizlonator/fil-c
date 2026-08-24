/* ';! lod ptr' is a typo for ';! load ptr': an unrecognized annotation is
   rejected rather than silently dropped (dropping it could skip a barrier). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x1, [x0]            ;! lod ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

