# Multi-and alias-crossing negative: two mid-function `and $-32, %rsp` notes.
# %rax parks rsp BETWEEN the two ands (after and#1, before and#2) and is used
# AFTER and#2. That crosses and#2's dynamic slack (0..31): pre- and post-and
# traffic share one numbering while each and shifts rsp, so the same slot key
# names different addresses on the two sides. The single-and check (save before
# firstAnd vs use after it) passes here — save is after and#1 — so this must be
# checked against the NEAREST preceding and to the use (and#2), which rejects
# it. See also sarcasm-frame-and-alias-after2-att (save after and#2, use after
# it — same side, legal).
	.text
	.globl	crossand2
	.type	crossand2, @function
crossand2:                       ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Lafter1
.Lafter1:
	and	$-32, %rsp
	movq	%rsp, %rax
	movq	%rdi, 0(%rax)
	jmp	.Lafter2
.Lafter2:
	and	$-32, %rsp
	movq	%rsi, 8(%rax)
	movq	0(%rax), %rcx
	addq	8(%rax), %rcx
	movq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	crossand2, .-crossand2
	.section	.note.GNU-stack,"",@progbits
