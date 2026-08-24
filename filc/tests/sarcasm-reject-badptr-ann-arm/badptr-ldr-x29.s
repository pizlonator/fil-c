/* An x29-relative access in a frame-pointer function is a virtualized frame
   slot: `;! load ptr` on [x29, ...] is rejected at the annotation site, same
   as sp. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	ldr	x0, [x29, #8]   ;! load ptr
	ldp	x29, x30, [sp], #32
	ret
	.size	f, .-f
