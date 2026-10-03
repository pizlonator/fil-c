# Feature 3 ({k}-masked stack-frame accesses), load side: a masked load from a
# stack slot lowers into an unmasked full-width load plus the register-form
# masked move, so MERGE-masking keeps the destination register's masked-off
# lanes and zero-masking ({z}) zeroes them. The area is filled with -1; a
# {%k1} (lanes 0-3) merge load over a destination preloaded with -1 must
# leave lane 4 at -1; the {z} variant must zero it; lane 0 must read the
# memory (-1) in both. Returns 0 iff all three hold.
	.text
	.globl	maskedload
	.type	maskedload, @function
maskedload:                     ;! long(long)
	subq	$128, %rsp
	vpxord	%zmm0, %zmm0, %zmm0
	vpternlogd	$0xFF, %zmm1, %zmm1, %zmm1	# zmm1 = all ones
	vmovdqu64	%zmm1, 0(%rsp)		# the area = -1 in every lane
	# merge-masking variant
	vmovdqu64	%zmm1, %zmm3		# dst = -1 everywhere
	movl	$15, %eax
	kmovw	%eax, %k1			# lanes 0-3
	vmovdqu64	0(%rsp), %zmm3{%k1}	# merge load: lanes 0-3 = mem (-1), 4-7 keep -1
	vpextrq	$0, %xmm3, %rsi			# lane 0 = -1 (loaded from memory)
	vextracti32x4	$2, %zmm3, %xmm4	# qword lanes 4-5
	vpextrq	$0, %xmm4, %rcx			# lane 4 = -1 (merge preserved)
	# zero-masking variant
	vmovdqu64	%zmm1, %zmm5		# dst = -1 everywhere
	vmovdqu64	0(%rsp), %zmm5{%k1}{z}	# lanes 0-3 = -1, lanes 4-7 = 0
	vextracti32x4	$2, %zmm5, %xmm6	# qword lanes 4-5
	vpextrq	$0, %xmm6, %rdx			# lane 4 = 0 (zeroed)
	xorl	%eax, %eax
	addq	$1, %rsi			# 0 iff lane 0 was -1 (loaded)
	testq	%rsi, %rsi
	jz	.Lc1
	orq	$1, %rax
.Lc1:	addq	$1, %rcx			# 0 iff merge preserved lane 4
	testq	%rcx, %rcx
	jz	.Lc2
	orq	$2, %rax
.Lc2:	testq	%rdx, %rdx			# 0 iff {z} zeroed lane 4
	jz	.Lc3
	orq	$4, %rax
.Lc3:	addq	$128, %rsp
	ret
	.size	maskedload, .-maskedload
	.section	.note.GNU-stack,"",@progbits
