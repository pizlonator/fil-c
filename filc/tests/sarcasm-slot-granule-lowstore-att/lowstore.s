	.text
# A 32-bit store to offset 0 of an 8-byte slot web, then a full 8-byte read:
# memory semantics preserve the HIGH dword the 8-byte store wrote (the narrow
# store is a read-modify-write of the granule web). This is the documented
# same-offset slot-virtualization gap (x86_64_isa partialRegWrite): the 32-bit
# store used to zero-extend the whole web, zeroing the high dword.
	.globl	lowstore
	.type	lowstore, @function
lowstore:                       ;! unsigned(long)
	subq	$16, %rsp
	movabsq	$0x1111222233334444, %rax
	movq	%rax, 0(%rsp)
	movl	$0xdeadbeef, %eax
	movl	%eax, 0(%rsp)
	movq	0(%rsp), %rax      # 0x11112222deadbeef
	addq	$16, %rsp
	ret
	.size	lowstore, .-lowstore
	.section	.note.GNU-stack,"",@progbits
