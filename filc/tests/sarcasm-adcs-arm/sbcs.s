/* sbcs (x - y - 1 + C) reads the carry-in like adcs, so the instrumented ldr
   between the cmp and the sbcs must keep its mrs/msr nzcv bracket: without it
   the access check's compare clobbers CF and the borrow is wrong. Companion
   of adcs.s (the load-bearing flag-liveness coverage for this family). */
	.text
	.global	g
	.type	g, %function
g:                              ;! long(ptr, long, long)
	cmp	x1, x2
	ldr	x3, [x0]
	sbcs	x1, x1, x2
	adds	x3, x3, #1
	add	x0, x1, x3
	ret
	.size	g, .-g
	.section	.note.GNU-stack,"",@progbits
