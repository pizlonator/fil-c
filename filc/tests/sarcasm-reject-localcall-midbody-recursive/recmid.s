# A recursive mid-body call: the mid-body label's clone range calls itself.
# With per-callsite cloning the continuations are fixed at compile time, so a
# re-entrant activation could never find its inner continuation — the same
# static rejection as a recursive top-level local subroutine.
	.text
	.globl	rec_fn
	.type	rec_fn, @function
rec_fn:                         ;! long(long)
	movq	%rdi, %r10
	call	.Lmid_rec
	movq	%r9, %rax
	ret
	.align	16
.Lmid_rec:
	testq	%r10, %r10
	jz	.Lout_rec
	leaq	-1(%r10), %r10
	call	.Lmid_rec           # recursion: rejected at discovery
	leaq	1(%r9), %r9
	ret
	.align	16
.Lout_rec:
	xorl	%r9d, %r9d
	ret
	.size	rec_fn, .-rec_fn
	.section	.note.GNU-stack,"",@progbits