# A counted LOOP inside a B2 clone: cl_top unconditionally joins cl_own's
# region, whose body loops (backward branch, dec/jnz consuming fresh flags
# inside the clone) while accumulating carried registers from the jumper.
# The loop head is a label INSIDE the cloned region and the loop exits to the
# clone's own tail (a plain ret = return from the jumper).
	.text
	.globl	cl_top
	.type	cl_top, @function
cl_top:                         ;! long(long,long,long)
	# %rdi = a (addend), %rsi = count, %rdx = step
	movq	%rsi, %r10
	movq	%rdx, %r11
	xorl	%r9d, %r9d
	jmp	.Lcl_entry
	.size	cl_top, .-cl_top
	.globl	cl_top2
	.type	cl_top2, @function
cl_top2:                        ;! long(long,long,long)
	# zero-count jumper: the loop body must not run at all
	movq	%rsi, %r10
	movq	%rdx, %r11
	xorl	%r9d, %r9d
	xorl	%r10d, %r10d
	jmp	.Lcl_entry
	.size	cl_top2, .-cl_top2
	.globl	cl_own
	.type	cl_own, @function
cl_own:                         ;! long(long,long,long)
	nop
.Lcl_entry:
	# guard: a zero count skips the body entirely
	testq	%r10, %r10
	jz	.Lcl_done
.Lcl_loop:
	# accumulate a + step each iteration, count times
	addq	%rdi, %r9
	addq	%r11, %r9
	decq	%r10
	jnz	.Lcl_loop
.Lcl_done:
	# one final add after the loop (so a zero count still sees one add)
	addq	%rdi, %r9
	movq	%r9, %rax
	ret
	.size	cl_own, .-cl_own
	.section	.note.GNU-stack,"",@progbits
