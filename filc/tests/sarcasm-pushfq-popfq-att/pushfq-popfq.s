	.text

# ---------------------------------------------------------------------------
# pushfq/popfq restore semantics: the materialized word must hold the EXACT
# pending condition the program had at the pushfq, even though instructions
# between the pushfq and the popfq write flags (their writes are dead in the
# model - the popfq fully rewrites EFLAGS), and the consumer after the popfq
# must read the RESTORED flags. Every branch direction below is observable
# from pushfq-popfq-main.c's return value.

# %rdi = a value. ZF <- (rdi == 0); the flags are saved, clobbered (the addq
# and incq are kept by DCE: their %rsi result is returned), restored by popfq,
# and read by je. Returns 111 when the restored ZF says a != 0, 222 when it
# says a == 0.
	.globl	pf_eq_restore
	.type	pf_eq_restore, @function
pf_eq_restore:                  ;! long(long, long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	addq	$17, %rsi        # clobbers SF/ZF/CF/OF/PF/AF (kept: %rsi returned)
	incq	%rsi             # clobbers all but CF (kept: %rsi returned)
	popfq
	je	.Leqr_waszero
	movq	%rsi, %rax
	ret
.Leqr_waszero:
	movq	%rsi, %rax
	addq	$111, %rax
	ret
	.size	pf_eq_restore, .-pf_eq_restore

# Same shape for the sign flag (js/jns) and the carry flag (jc/jnc): the
# producer materializes SF/CF from %rdi, the clobber is the %rsi arithmetic,
# and the popfq feeds the conditional branch.
	.globl	pf_s_restore
	.type	pf_s_restore, @function
pf_s_restore:                   ;! long(long, long)
	movq	%rdi, %rax
	shlq	$63, %rax        # SF <- bit 63 of a
	pushfq
	addq	$17, %rsi        # clobber (kept: %rsi returned)
	popfq
	js	.Lsr_neg
	movq	%rsi, %rax
	ret
.Lsr_neg:
	movq	%rsi, %rax
	addq	$222, %rax
	ret
	.size	pf_s_restore, .-pf_s_restore

	.globl	pf_c_restore
	.type	pf_c_restore, @function
pf_c_restore:                   ;! long(long, long)
	movq	%rdi, %rax
	subq	$16, %rax        # CF <- (a < 16) (unsigned borrow)
	pushfq
	addq	$17, %rsi        # clobber (kept: %rsi returned)
	popfq
	jc	.Lcr_borrow
	movq	%rsi, %rax
	ret
.Lcr_borrow:
	movq	%rsi, %rax
	addq	$333, %rax
	ret
	.size	pf_c_restore, .-pf_c_restore

# A pushfq IMMEDIATELY followed by popfq is the identity: the flags the
# consumer after the popfq reads are exactly the producer's.
	.globl	pf_identity
	.type	pf_identity, @function
pf_identity:                    ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	popfq
	je	.Lid_zero
	movq	$1, %rax
	ret
.Lid_zero:
	movq	$0, %rax
	ret
	.size	pf_identity, .-pf_identity

# Flags round-trip through a REGISTER: pushfq; pop %r10 parks the word in a
# register across intervening code, then push %r10; popfq restores it. The
# intervening code includes flag-writing arithmetic whose result is returned,
# so the DCE keeps it and the model must still restore the saved word, not the
# fresh flags.
	.globl	pf_reg_roundtrip
	.type	pf_reg_roundtrip, @function
pf_reg_roundtrip:               ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	popq	%r10             # the word lives in %r10 now
	movq	$1000, %r11
	addq	%rax, %r11       # flags clobber (kept: %r11 returned)
	cmpq	$7, %r11         # flags clobber (dead flag writes, DCE may drop it)
	pushq	%r10
	popfq
	je	.Lrr_even        # reads the RESTORED ZF from (a == 0)
	movq	%r11, %rax
	addq	$11, %rax
	ret
.Lrr_even:
	movq	%r11, %rax
	addq	$22, %rax
	ret
	.size	pf_reg_roundtrip, .-pf_reg_roundtrip

# Reading the word back through the stack: pushfq then an ordinary pop of a
# caller-saved register materializes the flags value as an INTEGER the program
# can observe (low 12 bits of EFLAGS; the high bits are reserved/undefined, so
# only the modeled bits are compared). The add/cmp between pushfq and the pop
# write flags but must not change the saved word.
	.globl	pf_word_readback
	.type	pf_word_readback, @function
pf_word_readback:               ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	addq	$5, %rdi         # clobber (kept)
	popq	%r11             # %r11 <- the saved EFLAGS word
	andq	$64, %r11        # isolate ZF (EFLAGS bit 6)
	movq	%r11, %rax
	ret
	.size	pf_word_readback, .-pf_word_readback
	.section	.note.GNU-stack,"",@progbits
