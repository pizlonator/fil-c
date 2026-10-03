# Feature 3 ({k}-masked stack-frame accesses): a masked VECTOR-MOVE store to a
# stack slot lowers exactly into unmasked load / register-masked move /
# unmasked store (merge-masking over the slot's old bytes), so masked-off
# lanes PRESERVE the memory. The test fills the 64-byte area with -1, stores
# zeros with {%k1} = 0x0F (qword lanes 0-3 only), and reads back: lane 0 must
# be 0 (masked-on, written), lane 4 must still be -1 (masked-off, preserved).
# Returns -1 on success, a distinct positive code per failed observation.
	.text
	.globl	maskedstore
	.type	maskedstore, @function
maskedstore:                    ;! long(long)
	subq	$128, %rsp
	vpxord	%zmm0, %zmm0, %zmm0		# zmm0 = 0 (the stored value)
	vpternlogd	$0xFF, %zmm1, %zmm1, %zmm1	# zmm1 = all ones (the fill)
	vmovdqu64	%zmm1, 0(%rsp)		# the area = -1 in every lane
	vmovdqu64	0(%rsp), %zmm2		# read the fill back
	vmovq	%xmm2, %rsi			# lane 0 of the fill (-1)
	movl	$15, %eax
	kmovw	%eax, %k1			# mask: qword lanes 0-3 only
	vmovdqu64	%zmm0, 0(%rsp){%k1}	# masked store of zeros (lowers exactly)
	vmovdqu64	0(%rsp), %zmm3		# read the whole area back
	vmovq	%xmm3, %rdi			# lane 0: must be 0 now
	vextracti32x4	$2, %zmm3, %xmm4	# qword lanes 4-5
	vmovq	%xmm4, %rcx			# lane 4: must still be -1
	movq	%rsi, %rax
	addq	$1, %rax			# 0 iff the fill was -1
	testq	%rax, %rax
	jnz	.Lbad_fill
	testq	%rdi, %rdi
	jnz	.Lbad_store
	movq	%rcx, %rax			# -1 when the masked-off lane is preserved
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
	.size	maskedstore, .-maskedstore
	.section	.note.GNU-stack,"",@progbits
