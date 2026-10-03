	.text
	# Feature (D0 interior lea-save carriers), the re-park path: the chacha 8x
	# body re-leas its carrier register mid-body after it served as a loop
	# counter. Both leas run at the prologue depth (d == D0); each scan walks
	# the other as a foreign park (a definition of the register), so both park
	# independently and the two carriers' accesses key their own K-displaced
	# slots (the second park's -32(%rbx) is the same address the first park's
	# 0(%rbx) store wrote: 32(%rsp) both ways). The redefinition between the
	# parks is a 32-bit write — the same modeled-def rule the smap's
	# def-marking uses.
	.globl	lead0repark
	.type	lead0repark, @function
lead0repark:                    ;! long(long)
	subq	$128, %rsp
	leaq	32(%rsp), %rbx     # first park (parked depth 96)
	movq	%rdi, 0(%rbx)      # carrier store -> slot 32
	movl	$7, %ebx           # redefine %rbx (the loop-counter shape)
	leaq	64(%rsp), %rbx     # re-park at a different K (parked depth 64)
	movq	-32(%rbx), %rax    # carrier load: 0-32+128-64 -> slot 32 (the first store)
	addq	%rax, %rax         # 2*x
	movq	%rax, 0(%rbx)      # carrier store -> slot 64
	movq	0(%rbx), %rax      # carrier load -> slot 64
	addq	$128, %rsp
	ret
	.size	lead0repark, .-lead0repark
	.section	.note.GNU-stack,"",@progbits
