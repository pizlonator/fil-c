	.text
	# Feature (D0 interior lea-save carriers), the lea-save target window: the
	# scan proves the destination register's uses memory-only (each access
	# through the carrier resolves inside the frame), so the lea is recognized
	# as a carrier — and then the lea-save target window rejects the ANCHOR
	# itself: the lea's normalized offset (128) lies outside the frame's real
	# extent [0, 64). Taking the address of the stack frame cannot be proven
	# safe, no matter how innocuous the reads through the parked value are.
	.globl	lead0win
	.type	lead0win, @function
lead0win:                       ;! long(long)
	subq	$64, %rsp
	leaq	128(%rsp), %rbx    # anchor at normalized 128 >= frameSize 64
	movq	%rdi, -128(%rbx)   # the use resolves to normalized 0 (inside)
	movq	-128(%rbx), %rax
	addq	$64, %rsp
	ret
	.size	lead0win, .-lead0win
	.section	.note.GNU-stack,"",@progbits
