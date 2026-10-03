	.text
	# Feature (D0 interior lea-save carriers), the CONSERVATIVE side: a
	# prologue-depth interior lea whose destination register has a VALUE use
	# (here the register is copied into the call's argument) is NOT a carrier —
	# the scan falls back and the historical D9 escape promotion runs, turning
	# the whole fixed frame into a GC region. The lea then materializes a real
	# region pointer (a valid capability), so the value use hands the callee a
	# pointer into the promoted region, the helper's stores are runtime-checked
	# region traffic, and the same-offset reads observe them. This pins today's
	# promoted semantics end to end: the pointer is real, the traffic is
	# checked, and the values round-trip.
	.globl	lead0val
	.type	lead0val, @function
lead0val:                       ;! long(long)
	pushq	%rbx
	subq	$96, %rsp
	leaq	32(%rsp), %rbx     # D0 interior lea with a VALUE use below
	movq	%rbx, %rdi         # value read: the register escapes into the call
	call	fill48 ;! void(ptr)
	movq	32(%rsp), %rax     # promoted-region traffic: helper's qword 0
	addq	40(%rsp), %rax     # promoted-region traffic: helper's qword 1
	addq	$96, %rsp
	popq	%rbx
	ret
	.size	lead0val, .-lead0val
	.globl	fill48
	.type	fill48, @function
fill48:                         ;! void(ptr)
	movq	$100, (%rdi)
	movq	$101, 8(%rdi)
	movq	$102, 16(%rdi)
	movq	$103, 24(%rdi)
	movq	$104, 32(%rdi)
	movq	$105, 40(%rdi)
	ret
	.size	fill48, .-fill48
	.section	.note.GNU-stack,"",@progbits
