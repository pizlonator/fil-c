/* A post-index writeback with a REGISTER index is not part of the modeled
   load/store subset (on arm64 the form only exists for SIMD ld1/st1): the
   access check cannot size the writeback, so it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr, long)
	ldr	x2, [x0], x1
	mov	x0, x2
	ret
	.size	f, .-f
