	.text
# Two adjacent 8-byte granules (offsets 0 and 8) with narrow traffic into each:
# no cross-talk — each narrow access routes to ITS OWN granule's web.
	.globl	twogran
	.type	twogran, @function
twogran:                        ;! unsigned(long)
	subq	$32, %rsp
	movabsq	$0x1111111122222222, %rax
	movq	%rax, 0(%rsp)
	movabsq	$0x3333333344444444, %rax
	movq	%rax, 8(%rsp)
	movl	4(%rsp), %edx      # high dword of granule 0: 0x11111111
	movl	12(%rsp), %ecx     # high dword of granule 1: 0x33333333
	movl	%edx, %eax
	addl	%ecx, %eax         # 0x44444444
	addq	$32, %rsp
	ret
	.size	twogran, .-twogran
	.section	.note.GNU-stack,"",@progbits
