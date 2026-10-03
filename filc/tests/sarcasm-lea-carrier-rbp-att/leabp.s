# Feature (lea-save carriers at provable depths, %rbp destination): with no
# frame pointer established, `leaq 24(%rsp), %rbp` parks a carrier in %rbp
# exactly like any other register (the same freedom carrierCopyOf grants the
# rsaz-avx2 `movq %rax,%rbp` shape). The accesses through it resolve
# statically to ordinary slots; the carrier itself is dropped and %rbp never
# becomes a frame pointer (the rbp-relative access machinery stays out of the
# picture — every access here rides the carrier).
	.text
	.globl	leabp
	.type	leabp, @function
leabp:                          ;! long(long)
	subq	$64, %rsp
	leaq	24(%rsp), %rbp
	movq	%rdi, -24(%rbp)		# slot [-64)
	movq	%rdi, 8(%rbp)		# slot [-32)
	movq	-24(%rbp), %rax
	addq	8(%rbp), %rax		# 2*rdi
	addq	$64, %rsp
	ret
	.size	leabp, .-leabp
	.section	.note.GNU-stack,"",@progbits
