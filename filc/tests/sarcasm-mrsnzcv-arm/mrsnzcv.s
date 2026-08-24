/* `mrs xN, nzcv` reads the condition flags, so an instrumented access between the
   program's flag writer (cmp) and its mrs must be bracketed by mrs/msr nzcv (flag
   save/restore). The trailing `adds` (a full NZCV writer) is what makes the flag-
   liveness scan stop at a DEF unless mrs is recognized as a USE: without the
   bracket the program's mrs would read the access check's flags instead of the
   cmp's. Returns the cmp's signed-less-than bit (N^V) extracted from the mrs
   result: f(p, 3) == 1, f(p, 9) == 0. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr, long)
	cmp	x1, #5
	ldr	x2, [x0]
	mrs	x3, nzcv
	adds	x2, x2, #1
	ubfx	x4, x3, #31, #1
	ubfx	x5, x3, #28, #1
	eor	x0, x4, x5
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
