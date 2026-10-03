# A branch out of a mid-body clone's range. The clone of .Lmid_esc must stay
# inside the owner function's text; its conditional jump leaves for .Lout, a
# label of a top-level SUBROUTINE region — for a mid-body clone that is a
# tail join whose +8 context would be the owner's frame convention executing
# in a possibly foreign activation, so discovery rejects it (the mont5
# shape — a tail join from a top-level subroutine — stays supported).
	.text
	.globl	esc_fn
	.type	esc_fn, @function
esc_fn:                         ;! long(long)
	movq	%rdi, %r10
	call	.Lmid_esc
	movq	%r9, %rax
	ret
	.align	16
.Lmid_esc:
	testq	%r10, %r10
	jz	.Lout                # escapes the mid-body region -> rejected
	leaq	(%r10,%r10), %r9
	ret
	.size	esc_fn, .-esc_fn
	.type	outer_sub, @function
outer_sub:
.Lout:
	movq	%r10, %r9
	ret
	.size	outer_sub, .-outer_sub
	.section	.note.GNU-stack,"",@progbits