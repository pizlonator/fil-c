	.text
# Feature B: stack buffers in localcall clones. The subroutine `_m1x1` keeps
# its own `sub` frame (its tab lives in the clone's own sub-frame area); its
# first indexed read carries the LONG form (the buffer's canonical declaration,
# in the subroutine's own frame coordinates) and the rest are short forms
# resolved through the clone's own keying. Three localcalls from one caller.
	.globl	lc_top
	.type	lc_top, @function
lc_top:                         ;! long(long,long)
	pushq	%rbx
	subq	$32, %rsp
	# callsite 1: a=3, b=5 -> tab[3] + tab[5] = 30 + 50 = 80
	movl	%edi, %r10d
	movl	%esi, %r11d
	call	m1x1
	movq	%r9, %rbx
	# callsite 2: a=6, b=7 -> tab[6] + tab[7] = 60 + 70 = 130
	movl	$6, %r10d
	movl	$7, %r11d
	call	m1x1
	addq	%r9, %rbx
	# callsite 3: a=15, b=0 -> tab[15] + tab[0] = 150 + 0 = 150
	movl	$15, %r10d
	xorl	%r11d, %r11d
	call	m1x1
	addq	%r9, %rbx
	movq	%rbx, %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	lc_top, .-lc_top
	.type	m1x1, @function
m1x1:
	subq	$160, %rsp
	# seed the 16-entry tab: entry i = 10*i
	movl	$0, (%rsp)
	movl	$10, 4(%rsp)
	movl	$20, 8(%rsp)
	movl	$30, 12(%rsp)
	movl	$40, 16(%rsp)
	movl	$50, 20(%rsp)
	movl	$60, 24(%rsp)
	movl	$70, 28(%rsp)
	movl	$80, 32(%rsp)
	movl	$90, 36(%rsp)
	movl	$100, 40(%rsp)
	movl	$110, 44(%rsp)
	movl	$120, 48(%rsp)
	movl	$130, 52(%rsp)
	movl	$140, 56(%rsp)
	movl	$150, 60(%rsp)
	# the indexed reads: tab[a] + tab[b] (each dword at tab base + 4*i)
	movl	(%rsp,%r10,4), %r9d #! stack buffer (tab, %rsp, %rsp + 64)
	movl	(%rsp,%r11,4), %eax #! stack buffer (tab)
	addl	%eax, %r9d
	addq	$160, %rsp
	ret
	.size	m1x1, .-m1x1
	.section	.note.GNU-stack,"",@progbits
