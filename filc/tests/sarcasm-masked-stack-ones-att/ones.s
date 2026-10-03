# Feature 3: a masked store whose mask is ALL-ONES lowers through the same
# exact sequence (load old / register-masked move / store full) — masking with
# all-ones is semantically the identity, so the lowered blend writes every
# lane with the new value. The test fills the area with 7s, stores -1 with
# {%k1} = 0xFF, and checks lanes 0 and 6 read back as -1. Returns -1 on
# success, a distinct positive code per failed observation.
	.text
	.globl	ones
	.type	ones, @function
ones:                           ;! long(long)
	subq	$128, %rsp
	vpxord	%zmm0, %zmm0, %zmm0		# 0
	vpternlogd	$0xFF, %zmm1, %zmm1, %zmm1	# -1 (the stored value)
	movq	$7, %rax
	vmovq	%rax, %xmm2
	vpbroadcastq	%xmm2, %zmm2		# 7 in every lane (the fill)
	vmovdqu64	%zmm2, 0(%rsp)		# area = 7 everywhere
	vmovdqu64	0(%rsp), %zmm3		# read the fill back
	vmovq	%xmm3, %rsi			# lane 0 = 7
	movl	$0xFF, %eax
	kmovw	%eax, %k1			# ALL lanes on
	vmovdqu64	%zmm1, 0(%rsp){%k1}	# masked store of -1 (all lanes)
	vmovdqu64	0(%rsp), %zmm4		# read back
	vmovq	%xmm4, %rdi			# lane 0 = -1
	vextracti32x4	$3, %zmm4, %xmm5	# qword lanes 6-7
	vmovq	%xmm5, %rcx			# lane 6 = -1
	movq	%rsi, %rax
	subq	$7, %rax			# 0 iff the fill was 7
	testq	%rax, %rax
	jnz	.Lbad_fill
	notq	%rdi				# 0 iff lane 0 was -1
	jnz	.Lbad_store
	notq	%rcx				# 0 iff lane 6 was -1
	jnz	.Lbad_store
	movl	$1, %eax
	negq	%rax				# -1 = success
	addq	$128, %rsp
	ret
.Lbad_fill:
	movl	$2, %eax
	addq	$128, %rsp
	ret
.Lbad_store:
	movl	$4, %eax
	addq	$128, %rsp
	ret
	.size	ones, .-ones
	.section	.note.GNU-stack,"",@progbits
