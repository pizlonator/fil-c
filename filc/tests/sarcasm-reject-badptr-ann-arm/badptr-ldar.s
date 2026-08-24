/* An atomic load cannot carry a capability (atomics are not in the scalar
   ldr/str family the annotation is defined on). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldar	x0, [x1]            ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

