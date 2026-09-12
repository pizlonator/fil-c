# High-byte registers %ah/%ch/%dh/%bh: reads extract bits 8-15 of the enclosing
# register's web; writes merge into bits 8-15 and preserve every other bit.
# The pin machinery (emitPinned) lands the enclosing web in the spelled physical
# register, so the spelled instruction operates on the architected byte wherever
# regalloc colored the webs — exercised here under register pressure (every
# allocatable GPR held live across the high-byte operations).
	.text
	.globl	hb_ops
	.type	hb_ops, @function
hb_ops:                         ;! void(ptr)
	endbr64
	# pressure values that must survive untouched
	movq	$0x1111111111111111, %r12
	movq	$0x2222222222222222, %r13
	movq	$0x3333333333333333, %r14
	movq	$0x4444444444444444, %r15
	movq	$0x5555555555555555, %rbp
	# movzbl reads of every high-byte register
	movq	$0x1122334455667788, %rax
	movq	$0x2233445566778899, %rcx
	movq	$0x33445566778899AA, %rdx
	movq	$0x445566778899AABB, %rbx
	movzbl	%ah, %esi
	movl	%esi, (%rdi)
	movzbl	%ch, %esi
	movl	%esi, 4(%rdi)
	movzbl	%dh, %esi
	movl	%esi, 8(%rdi)
	movzbl	%bh, %esi
	movl	%esi, 12(%rdi)
	# movzbl into a callee-saved destination web (pinned def), read it back later
	# movb writes: low byte into high byte (merge, preserving the rest)
	movb	%cl, %ah
	movq	%rax, 16(%rdi)
	# high byte into low byte (also a merge into the partner)
	movb	%dh, %al
	movq	%rax, 24(%rdi)
	movq	%rcx, 32(%rdi)
	# high byte into high byte
	movb	%bh, %ch
	movq	%rcx, 40(%rdi)
	# immediate into high byte
	movb	$0x5A, %dh
	movq	%rdx, 48(%rdi)
	# byte ALU on a high-byte register (RMW of the enclosing web)
	andb	$0x0F, %ah
	movq	%rax, 56(%rdi)
	orb	$0x10, %ch
	movq	%rcx, 64(%rdi)
	# cmpb/testb with a high-byte operand (flags only)
	cmpb	$0x09, %ah
	setne	%r11b
	movzbl	%r11b, %eax
	movq	%rax, 72(%rdi)
	testb	$0x04, %ch
	setz	%r11b
	movzbl	%r11b, %eax
	movq	%rax, 80(%rdi)
	# inc/dec/neg on a high byte
	movq	$0x0102030405060708, %rax
	incb	%ah
	movq	%rax, 88(%rdi)
	decb	%dh
	movq	%rdx, 96(%rdi)
	negb	%bh
	movq	%rbx, 104(%rdi)
	# xchgb swaps the high bytes of two registers
	movq	$0x1111111111112233, %rax
	movq	$0x2222222222224455, %rcx
	xchgb	%ah, %ch
	movq	%rax, 112(%rdi)
	movq	%rcx, 120(%rdi)
	# a movzbl read of the ALU-modified %ch (0xBA), and the pressure registers
	movzbl	%ch, %esi
	movl	%esi, 128(%rdi)
	movq	%r12, 136(%rdi)
	movq	%r13, 144(%rdi)
	movq	%r14, 152(%rdi)
	movq	%r15, 160(%rdi)
	movq	%rbp, 168(%rdi)
	ret
	.size	hb_ops, .-hb_ops

	# xor-zeroing followed by a HIGH-byte def and a full-width use: the movb
	# merges into bits 8-15 and preserves the rest (zeroed above), so the xor
	# must stay live across the high-byte write. (The source must be REX-free
	# to pair with %ah: %cl here.)
	.globl	hb_xor_ah
	.type	hb_xor_ah, @function
hb_xor_ah:                      ;! long(long)
	endbr64
	movq	%rdi, %rcx
	movq	$0x1122334455667788, %rax
	xorq	%rax, %rax
	movb	%cl, %ah
	ret
	.size	hb_xor_ah, .-hb_xor_ah
	.section	.note.GNU-stack,"",@progbits
