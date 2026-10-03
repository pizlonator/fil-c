# Register webs spanning a B2 join: rw_top defines %r12 (64-bit), %r13
# (64-bit, used only at the very end — live across the whole clone) and a
# 32-bit partial web %r11d, then unconditionally joins rw_owner's region.
# The clone redefines every web (%r12 += 1, %r11d += 5), sign-extends the
# partial web, and combines everything with the caller's %rdi, which stays
# live across the join too. rw_top2 redefines %r12 BEFORE the join.
	.text
	.globl	rw_top
	.type	rw_top, @function
rw_top:                         ;! long(long,long,long)
	# %rdi = a, %rsi = b, %rdx = c
	movq	%rsi, %r12
	movq	%rdx, %r13
	movl	%edi, %r11d
	jmp	.Lrw_mid
	.size	rw_top, .-rw_top
	.globl	rw_top2
	.type	rw_top2, @function
rw_top2:                        ;! long(long,long,long)
	# same, but %r12 is redefined by the caller before the join
	movq	%rsi, %r12
	addq	$10, %r12
	movq	%rdx, %r13
	movl	%edi, %r11d
	jmp	.Lrw_mid
	.size	rw_top2, .-rw_top2
	.globl	rw_owner
	.type	rw_owner, @function
rw_owner:                       ;! long(long,long,long)
	nop
.Lrw_mid:
	# clone: redefine the carried webs, then combine.
	addq	$1, %r12
	addl	$5, %r11d
	movslq	%r11d, %r10
	# (r12 + 2*r13) + r10 + a
	leaq	(%r12,%r13,2), %rax
	addq	%r10, %rax
	addq	%rdi, %rax
	ret
	.size	rw_owner, .-rw_owner
	.section	.note.GNU-stack,"",@progbits
