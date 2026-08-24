/* autiasp implicitly read-modify-writes x30 (pointer-auth verify) with an
   unmodelable effect: rejected like paciasp (pointer-authentication
   instructions have implicit register effects that cannot be modeled). */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	mov	x0, x1
	autiasp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
