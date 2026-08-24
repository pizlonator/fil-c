/* ';! load ptr' on a store is a contradiction: the annotation must match the
   direction of the memory access. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	str	x0, [x1]            ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

