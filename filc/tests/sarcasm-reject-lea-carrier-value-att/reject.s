# Feature (lea-save carriers at provable depths) reject twin: reading a
# parked carrier AS A VALUE (any use other than the single memory base of a
# resolving stack access) observes the phantom save — the lea is dropped by
# the rewrite, so the register's stack value does not exist in the output.
# The relaxation only legalizes the memory-base spelling; the value read
# stays a hard error exactly as before. (The lea sits behind a dispatch
# merge label so it parks a carrier at a perturbed depth; at the prologue
# depth the D9 region promotion would own the shape instead. Note the
# carrier FLOWS — `movq %carrier, off(%rsp)` save-stores, slot loads, and
# reg-reg copies — remain legal dropped phantom traffic; this pins an
# OBSERVING use: the carrier's value handed to a heap store.)
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	testl	%edi, %edi
	jz	.Lskip
	jmp	.Lproceed
.Lproceed:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx
	movq	%rbx, (%rdi)		# value use of the carrier -> rejected
	addq	$64, %rsp
	ret
.Lskip:
	xorl	%eax, %eax
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits
