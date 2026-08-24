/* smc enters a higher exception level with RAW arguments and unmodeled
   control/clobber effects: rejected like svc (a raw supervisor call bypasses
   Fil-C's syscall argument validation). */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	mov	x0, x1
	smc	#0
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
