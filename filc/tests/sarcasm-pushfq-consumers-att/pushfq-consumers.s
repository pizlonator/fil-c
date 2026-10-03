	.text

# ---------------------------------------------------------------------------
# Consumers of the flags popfq DEFINES: setcc, adc, and cmov after a popfq
# read the restored condition. The popfq is a FULL flag write (all six), so
# these consumers see exactly the materialized word's bits - the tests pin
# both directions of each condition.

# pushfq around a flag-writing add (the word holds (a+1)==0's ZF), then setne
# materializes the restored ZF as an integer.
	.globl	po_setcc
	.type	po_setcc, @function
po_setcc:                       ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax         # ZF <- (a == -1); SF <- sign; CF <- carry out
	pushfq
	addq	$100, %rdi       # clobber flags (kept: %rdi folded into the result)
	popfq
	setne	%r10b            # reads the RESTORED ZF
	movzbl	%r10b, %r10d
	je	.Lsc_zero
	movq	%r10, %rax
	addq	$10, %rax
	ret
.Lsc_zero:
	movq	%r10, %rax
	addq	$20, %rax
	ret
	.size	po_setcc, .-po_setcc

# The restored CF feeds an adc: the carry-in of the adc is the CF from the
# materialized word, not the clobbering add's.
	.globl	po_adc
	.type	po_adc, @function
po_adc:                         ;! long(long, long)
	movq	%rdi, %rax
	subq	$16, %rax        # CF <- (a < 16)
	pushfq
	addq	$3, %rsi         # clobber CF (kept: %rsi is the addend)
	popfq
	adcq	$0, %rsi         # rsi += CF(restored)
	movq	%rsi, %rax
	ret
	.size	po_adc, .-po_adc

# The restored condition feeds a cmov: the select uses the restored ZF even
# though the intervening add wrote fresh flags.
	.globl	po_cmov
	.type	po_cmov, @function
po_cmov:                        ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	addq	$5, %rdi         # clobber (kept: both cmov sources derive from it)
	popfq
	movq	$111, %r10
	movq	$222, %r11
	cmove	%r10, %r11       # r11 = (restored ZF) ? 111 : 222
	movq	%r11, %rax
	addq	%rdi, %rax
	ret
	.size	po_cmov, .-po_cmov

# Everything-after-popfq reads DEFINED flags: the popfq is a full kill, so a
# jcc right after it consumes the word's bits with no save bracket needed
# (and the branch direction must match the word, not the clobbered hardware
# flags).
	.globl	po_defined
	.type	po_defined, @function
po_defined:                     ;! long(long)
	movq	%rdi, %rax
	shlq	$63, %rax        # SF <- bit 63
	pushfq
	addq	$9, %rdi         # clobber (kept)
	popfq
	js	.Ldf_neg
	movq	%rdi, %rax
	ret
.Ldf_neg:
	movq	%rdi, %rax
	addq	$500, %rax
	ret
	.size	po_defined, .-po_defined
	.section	.note.GNU-stack,"",@progbits
