	.text
	# Feature (D0 interior lea-save carriers), the window guard: a
	# prologue-depth interior lea whose anchor lies OUTSIDE the frame's real
	# extent is not a carrier — the lea-save target window rejects it (the
	# same interior rule the escape detector applies to the leas it counts).
	# The destination register's uses are memory-only, so this is the new
	# carrier path's own rejection, not the historical escape one.
	.globl	lead0oob
	.type	lead0oob, @function
lead0oob:                       ;! long(long)
	subq	$64, %rsp
	leaq	128(%rsp), %rbx    # normalized offset 128 >= frameSize 64
	movq	%rdi, 0(%rbx)
	movq	0(%rbx), %rax
	addq	$64, %rsp
	ret
	.size	lead0oob, .-lead0oob
	.section	.note.GNU-stack,"",@progbits
