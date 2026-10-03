# Feature (post-prologue-and carrier keying): a carrier parked AFTER the
# prologue `and $-32, %rsp` note holds the ROUNDED rsp, so its accesses are
# and-keyed (residue base = the note's depth) even when OTHER paths of the
# function carry mid-function `and`s (the sha1-mb shared-body shape). The
# aligned movdqa through the carrier lands on a 16-aligned synthesized home
# exactly when the keying is right — an 8-mod-16 home (the SysV residue the
# pre-fix code fell back to) would trap at runtime.
	.text
	.globl	parkpost
	.type	parkpost, @function
parkpost:                       ;! long(ptr)
	pushq	%rbx
	pushq	%rbp
	subq	$96, %rsp
	andq	$-32, %rsp		# prologue alignment note
	leaq	32(%rsp), %r10		# carrier parked after the note (rounded rsp)
	movq	%rdi, 24(%rsp)		# slot traffic: the prologue prefix ends here
	testq	%rdi, %rdi
	js	.Llate
.Lneg:
	movdqa	(%rdi), %xmm0		# 16 bytes from the (32-aligned) caller buffer
	movdqa	%xmm0, 0(%r10)		# aligned store through the carrier
	movdqa	0(%r10), %xmm1		# aligned reload
	vmovq	%xmm1, %r11
	cmpq	$17, %r11
	jne	.bad
	movl	$1, %eax
	addq	$96, %rsp
	popq	%rbp
	popq	%rbx
	ret
.bad:
	xorl	%eax, %eax
	addq	$96, %rsp
	popq	%rbp
	popq	%rbx
	ret
.Llate:
	# The mid-function and sits AFTER the carrier uses in body order (the
	# sha1-mb shared-body layout: the cloned bodies' ands are appended below
	# the jumper's carrier traffic), so the carrier's park precedes every
	# mid-and and its accesses key the prologue note's coordinates.
	andq	$-32, %rsp
	movq	$0, 32(%rsp)
	xorl	%eax, %eax
	addq	$96, %rsp
	popq	%rbp
	popq	%rbx
	ret
	.size	parkpost, .-parkpost
	.section	.note.GNU-stack,"",@progbits
