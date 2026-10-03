	.text
# An index that lands BELOW the buffer: with idx = 0xfffffffffffffff8 (-8 as
# an unsigned 64-bit value) the effective buffer-relative offset is negative.
# The single unsigned compare must trap (not wrap into the range), and the
# runtime must attribute it as ptr < lower.
	.globl	oob_below
	.type	oob_below, @function
oob_below:                      ;! int(size_t)
	subq	$64, %rsp
	movl	$0x41424344, 16(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp + 8, %rsp + 40)
	addq	$64, %rsp
	ret
	.size	oob_below, .-oob_below
	.section	.note.GNU-stack,"",@progbits
