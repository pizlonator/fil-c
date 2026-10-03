# Feature (lea-save carriers at provable depths): `leaq 16(%rsp), %rbx` behind
# the dispatch TARGET label parks a carrier at depth 64 (entry_rsp - 64) — the
# prologue scan stops at the targeted label, so D0 = 0 and the lea runs at a
# perturbed depth. Every access through the carrier resolves statically to the
# same normalized slot the equivalent rsp-relative spelling keys (no region
# promotion, no escape), so the function compiles with the carrier dropped and
# the accesses virtualized. Runs to prove the resolved slots carry the values.
	.text
	.globl	leadeep
	.type	leadeep, @function
leadeep:                        ;! long(long)
	testl	%edi, %edi
	jz	.Lskip
	jmp	.Lproceed
.Lproceed:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx
	movq	%rdi, -16(%rbx)		# slot [-64)
	movq	%rdi, 32(%rbx)		# slot [-16)
	movq	-16(%rbx), %rax
	addq	32(%rbx), %rax		# 2*rdi
	movq	%rax, 0(%rbx)		# slot [-48)
	movq	0(%rbx), %rax
	addq	$64, %rsp
	ret
.Lskip:
	xorl	%eax, %eax
	ret
	.size	leadeep, .-leadeep
	.section	.note.GNU-stack,"",@progbits
