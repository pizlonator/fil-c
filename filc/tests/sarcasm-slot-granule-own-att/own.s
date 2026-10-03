	.text
# Narrow traffic with NO containing 8-byte web: the 4-byte accesses at offsets
# 0 and 4 keep their own per-offset slot webs (the historical behavior — a
# granule only forms where 8-byte slot traffic exists), and read-after-write
# through one's own web works.
	.globl	ownweb
	.type	ownweb, @function
ownweb:                         ;! unsigned(long)
	subq	$16, %rsp
	movl	$0xcafebabe, %eax
	movl	%eax, 4(%rsp)
	movl	4(%rsp), %eax      # reads its own 4-byte web: 0xcafebabe
	movl	$0xfeedface, %edx
	movl	%edx, 0(%rsp)
	movl	0(%rsp), %ecx      # reads its own 4-byte web: 0xfeedface
	addl	%ecx, %eax         # 0xcafebabe + 0xfeedface = 0x1c9ce8abc -> low 0xc9ce8abc
	addq	$16, %rsp
	ret
	.size	ownweb, .-ownweb
	.section	.note.GNU-stack,"",@progbits
