/* xpaclri implicitly read-modify-writes x30 (strips the PAC from the link
   register) with an unmodelable effect: rejected like paciasp
   (pointer-authentication instructions have implicit register effects that
   cannot be modeled). */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	mov	x0, x1
	xpaclri
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
