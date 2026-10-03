	.section	.rodata
	.align	64
.Lpat11:
	.quad	17,17,17,17,17,17,17,17
.Lpat22:
	.quad	34,34,34,34,34,34,34,34
	.text
# Gap 3, the trap: the runtime bounds check on the index precedes the whole
# masked lowering sequence (the old-bytes load, the masked merge, the
# store-back), so an out-of-range index traps WITHOUT touching the buffer —
# the masked store never executes.
	.globl	idxmask_oob
	.type	idxmask_oob, @function
idxmask_oob:                     ;! long(size_t)
	subq	$160, %rsp
	vmovdqa64	.Lpat11(%rip), %zmm0
	vmovdqu64	%zmm0, 0(%rsp)
	vmovdqu64	%zmm0, 64(%rsp)
	vmovdqa64	.Lpat22(%rip), %zmm1
	movl	$15, %eax
	kmovw	%eax, %k1
	vmovdqa64	%zmm1, 0(%rsp,%rdi,1){%k1} #! stack buffer (b, %rsp, %rsp + 128)
	movq	0(%rsp,%rdi,1), %rax    #! stack buffer (b)
	addq	$160, %rsp
	ret
	.size	idxmask_oob, .-idxmask_oob
	.section	.note.GNU-stack,"",@progbits
