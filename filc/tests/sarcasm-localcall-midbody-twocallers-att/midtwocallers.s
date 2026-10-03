# Two DIFFERENT callers calling the SAME mid-body label of a signatured
# function, plus the owner itself calling it (the aesni self-call shape).
# Cloning is per-caller: each caller gets its own renamed copy of the
# reachable code, colored with the caller's registers — so caller-local
# register state (%r11 below) cannot leak across callers, and each caller's
# results stay independent. The region touches no frame slots (registers
# only), which is what makes the cross-function clones sound.
	.text
	.globl	call_a
	.type	call_a, @function
call_a:                         ;! long(long)
	movq	%rdi, %r10
	movq	$100, %r11
	call	.Lmid_mix
	leaq	(%r9,%r11), %rax
	ret
	.size	call_a, .-call_a
	.globl	call_b
	.type	call_b, @function
call_b:                         ;! long(long)
	movq	%rdi, %r10
	movq	$9000, %r11
	call	.Lmid_mix
	leaq	(%r9,%r11), %rax
	ret
	.size	call_b, .-call_b
	.globl	mid_owner
	.type	mid_owner, @function
mid_owner:                      ;! long(long)
	movq	%rdi, %r10
	movq	$7, %r11
	call	.Lmid_mix
	movq	%r9, %rax
	ret
	.align	16
.Lmid_mix:
	leaq	(%r10,%r10), %r9    # r9 = 2*x + r11 + 3 (r11 = the caller's own)
	addq	%r11, %r9
	addq	$3, %r9
	ret
	.size	mid_owner, .-mid_owner
	.section	.note.GNU-stack,"",@progbits