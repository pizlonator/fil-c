# One B2 clone entered from TWO sites in the same jumper: a conditional
# first-level B2 (je) and an unconditional one (jmp) both target tw_own's
# mid-body label. The per-jumper (source,label) clone cache must reuse the
# same clone for both sites, and each site's own register state (here %r10,
# which marks which site was taken) must reach the clone.
	.text
	.globl	tw_top
	.type	tw_top, @function
tw_top:                         ;! long(long,long,long)
	# %rdi = which, %rsi = a, %rdx = b
	xorl	%r10d, %r10d
	testq	%rdi, %rdi
	je	.Ltw_mid
	movl	$1, %r10d
	jmp	.Ltw_mid
	.size	tw_top, .-tw_top
	.globl	tw_own
	.type	tw_own, @function
tw_own:                         ;! long(long,long,long)
	nop
.Ltw_mid:
	# 3*a + 100*which + b
	movq	%rsi, %rax
	leaq	(%rax,%rax,2), %rax
	imulq	$100, %r10, %r10
	addq	%r10, %rax
	addq	%rdx, %rax
	ret
	.size	tw_own, .-tw_own
	.section	.note.GNU-stack,"",@progbits
