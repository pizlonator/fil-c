# One flag web surviving TWO B2 joins and a NESTED CONDITIONAL B2 exit.
# mj_top sets flags (cmp) and unconditionally joins mj_b's region; that
# region's je both consumes the caller's flags AND exits to a further clone
# (mj_c's region) — the exact "conditional branch inside a cloned region that
# then exits to a further B2 clone" shape from the mont5 dispatch. mj_c's je
# consumes the same flags after a second join. All arithmetic on the carried
# registers uses lea so the flag web is never disturbed. mj_top2 joins
# mj_c's region directly, exercising both flag polarities there.
	.text
	.globl	mj_top
	.type	mj_top, @function
mj_top:                         ;! long(long,long,long)
	# %rdi = a, %rsi = b, %rdx = c
	movq	%rsi, %r10
	movq	%rdx, %r11
	cmpq	%r11, %r10
	jmp	.Lmj_b
	.size	mj_top, .-mj_top
	.globl	mj_top2
	.type	mj_top2, @function
mj_top2:                        ;! long(long,long,long)
	# straight into mj_c's region with the same flags discipline
	movq	%rsi, %r10
	movq	%rdx, %r11
	cmpq	%r11, %r10
	jmp	.Lmj_c
	.size	mj_top2, .-mj_top2
	.globl	mj_b
	.type	mj_b, @function
mj_b:                           ;! long(long,long,long)
	nop
.Lmj_b:
	# nested conditional B2 exit on the CALLER's flags: taken iff b == c.
	je	.Lmj_c
	# not-equal fallthrough: a + 2*b (lea only)
	leaq	(%rdi,%r10,2), %rax
	ret
	.size	mj_b, .-mj_b
	.globl	mj_c
	.type	mj_c, @function
mj_c:                           ;! long(long,long,long)
	nop
.Lmj_c:
	# same flags again, after the second join.
	je	.Lmj_eq
	# not equal: a + 3*b
	leaq	(%r10,%r10,2), %rax
	addq	%rdi, %rax
	ret
.Lmj_eq:
	# equal: a + 4*b + 100
	leaq	(%rdi,%r10,4), %rax
	addq	$100, %rax
	ret
	.size	mj_c, .-mj_c
	.section	.note.GNU-stack,"",@progbits
