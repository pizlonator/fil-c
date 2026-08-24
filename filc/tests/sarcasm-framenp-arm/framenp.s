/* Frame virtualization of the non-temporal pair forms and ldpsw: stnp/ldnp
   model as their plain stp/ldp forms (stack slots are thread-local, so the
   non-temporal hint is immaterial), and ldpsw sign-extends both halves. */
	.text
	.globl	np_pair
	.type	np_pair, %function
np_pair:                        ;! long(long, long)
	sub	sp, sp, #32
	stnp	x0, x1, [sp, #8]
	ldnp	x2, x3, [sp, #8]
	add	x0, x2, x3
	add	sp, sp, #32
	ret
	.size	np_pair, .-np_pair
	.globl	swsum
	.type	swsum, %function
swsum:                          ;! long(long, long)
	sub	sp, sp, #32
	stnp	w0, w1, [sp, #8]
	ldpsw	x0, x1, [sp, #8]
	add	x0, x0, x1
	add	sp, sp, #32
	ret
	.size	swsum, .-swsum
	.globl	x29np
	.type	x29np, %function
x29np:                          ;! long(long, long)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	stnp	x0, x1, [x29, #8]
	ldnp	x2, x3, [x29, #8]
	add	x0, x2, x3
	ldp	x29, x30, [sp], #32
	ret
	.size	x29np, .-x29np
	.globl	x29sw
	.type	x29sw, %function
x29sw:                          ;! long(long, long)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	stnp	w0, w1, [x29, #8]
	ldpsw	x0, x1, [x29, #8]
	add	x0, x0, x1
	ldp	x29, x30, [sp], #32
	ret
	.size	x29sw, .-x29sw
	.section	.note.GNU-stack,"",@progbits
