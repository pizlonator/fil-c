	.text
# Narrow granule traffic beyond the plain mov cases:
#   * byte and word LOADS at non-zero sub-offsets (extract + width semantics:
#     movzbl/movzwl zero-extend exactly like their memory forms);
#   * a middle-straddle 4-byte store (bytes 2-5 of the granule: the keep mask
#     has bits at both ends and rides a materialized movabs constant);
#   * a narrow store issued with the program's flags LIVE across it (the
#     merge sequence is bracketed by the standard flag save/restore, so the
#     setb after the store still consumes the cmp's condition).
	.globl	byteword
	.type	byteword, @function
byteword:                       ;! unsigned(long)
	subq	$16, %rsp
	movabsq	$0xaabbccddeeff0011, %rax
	movq	%rax, 0(%rsp)
	movb	7(%rsp), %al       # top byte: 0xaa
	movzbl	%al, %edx
	movw	6(%rsp), %cx       # top word: 0xaabb
	movzwl	%cx, %ecx
	movl	%edx, %eax
	addl	%ecx, %eax         # 0xaa + 0xaabb = 0xab65
	addq	$16, %rsp
	ret
	.size	byteword, .-byteword
	.globl	strad
	.type	strad, @function
strad:                          ;! unsigned(long)
	subq	$16, %rsp
	movabsq	$0x1122334455667788, %rax
	movq	%rax, 0(%rsp)
	movl	$0xdeadbeef, %eax
	movl	%eax, 2(%rsp)      # middle straddle: bytes 2-5
	movq	0(%rsp), %rax      # 0x1122deadbeef7788
	addq	$16, %rsp
	ret
	.size	strad, .-strad
	.globl	liveflags
	.type	liveflags, @function
liveflags:                      ;! unsigned(long, long)
	subq	$16, %rsp
	movq	$0, 0(%rsp)        # the granule web
	cmpl	%esi, %edi         # flags live across the narrow store below
	movl	$0x99, 4(%rsp)     # narrow store into granule 0 (merge is bracketed)
	setb	%al                # consumes CF from the cmp
	movl	4(%rsp), %ecx      # the stored 0x99 (no later full-width overwrite)
	addl	%eax, %ecx
	movl	%ecx, %eax         # 0x99 + (edi < esi ? 1 : 0)
	addq	$16, %rsp
	ret
	.size	liveflags, .-liveflags
	.section	.note.GNU-stack,"",@progbits
