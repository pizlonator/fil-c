	.text
# The Whirlpool (wp-x86_64.pl) shape: a 4-byte load from the HIGH half of an
# 8-byte frame slot. The 8-byte store defines the granule web; the narrow load
# routes to the containing web and extracts bits 32-63 (shrq $32), so it reads
# the stored high dword instead of an undefined web. (This used to
# miscompile: the narrow load created its own never-defined web at offset 4,
# and the 8-byte store was dead code DCE'd away.)
	.globl	highload
	.type	highload, @function
highload:                       ;! unsigned(long)
	subq	$16, %rsp
	movabsq	$0x1234567890abcdef, %rax
	movq	%rax, 0(%rsp)
	movl	4(%rsp), %eax      # the high dword: 0x12345678
	addq	$16, %rsp
	ret
	.size	highload, .-highload
	.section	.note.GNU-stack,"",@progbits
