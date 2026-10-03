	.text
# A 4-byte STORE to the high half of an 8-byte slot, then a full 8-byte read:
# the read must see the stored high dword (a read-modify-write of the granule
# web). (Used to miscompile: the narrow store defined its own web at offset 4
# while the 8-byte read looked at the offset-0 web, which never saw it.)
	.globl	highstore
	.type	highstore, @function
highstore:                      ;! unsigned(long)
	subq	$16, %rsp
	movabsq	$0x1111222233334444, %rax
	movq	%rax, 0(%rsp)
	movl	$0xdeadbeef, %eax
	movl	%eax, 4(%rsp)
	movq	0(%rsp), %rax      # 0xdeadbeef33334444
	addq	$16, %rsp
	ret
	.size	highstore, .-highstore
	.section	.note.GNU-stack,"",@progbits
