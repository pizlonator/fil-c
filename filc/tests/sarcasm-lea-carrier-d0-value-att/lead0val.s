	.text
	# Feature (D0 interior lea-save carriers), the CONSERVATIVE side: a
	# prologue-depth interior lea whose destination register has a VALUE use
	# (here the register is copied into the call's argument) is NOT a carrier —
	# the scan falls out and the lea stays ordinary. This used to be accepted
	# by the D9 fixed-frame escape promotion, which promoted the whole fixed
	# frame to a GC region (filc_allocate) so the value use handed the callee a
	# real region pointer. That promotion was removed: sarcasm now REJECTS
	# taking the address of the stack frame at compile time, so this file
	# fails to compile with "taking address of stack frame is not supported
	# (cannot prove safety)" on the escaping lea.
	.globl	lead0val
	.type	lead0val, @function
lead0val:                       ;! long(long)
	pushq	%rbx
	subq	$96, %rsp
	leaq	32(%rsp), %rbx     # D0 interior lea with a VALUE use below: rejected
	movq	%rbx, %rdi         # value read: the register escapes into the call
	call	fill48 ;! void(ptr)
	movq	32(%rsp), %rax     # frame-slot read (the compile fails first)
	addq	40(%rsp), %rax     # frame-slot read
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
