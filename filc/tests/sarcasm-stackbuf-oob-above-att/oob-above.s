	.text
# An index past the buffer's upper bound: idx = 100 with a 4-byte access into
# a buffer whose usable range for this access ends at 36. The runtime must
# attribute ptr >= upper.
	.globl	oob_above
	.type	oob_above, @function
oob_above:                      ;! int(size_t)
	subq	$64, %rsp
	movl	$0x41424344, 16(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	oob_above, .-oob_above
	.section	.note.GNU-stack,"",@progbits
