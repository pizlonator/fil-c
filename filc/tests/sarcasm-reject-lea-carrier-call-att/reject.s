# Feature (lea-save carriers at provable depths) reject twin: a carrier parked
# in a CALLER-SAVED register (%r10) does not survive a hardware call (SysV
# clobbers it) — the call poisons the carrier, so recovering %rsp through it
# revives no provable depth and the `ret` at the unknown depth is statically
# rejected. Exactly the mov-save twin (sarcasm-reject-b2-carrier-call-att);
# the lea relaxation changes nothing about the call-clobber rule.
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$64, %rsp
	leaq	16(%rsp), %r10		# carrier parked in a caller-saved register
	call	rej_helper              ;! long(long)
	movq	%r10, %rsp		# poisoned carrier: revives no depth
	ret				# ret at an unknown depth -> rejected
	.size	rej, .-rej
	.globl	rej_helper
	.type	rej_helper, @function
rej_helper:                     ;! long(long)
	movq	%rdi, %rax
	ret
	.size	rej_helper, .-rej_helper
	.section	.note.GNU-stack,"",@progbits
