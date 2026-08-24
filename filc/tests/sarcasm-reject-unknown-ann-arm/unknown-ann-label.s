/* A ;! annotation on a mid-body label would be silently ignored (only the
   function-entry label carries the signature), so it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, [x0]
.Ldone:                         ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

