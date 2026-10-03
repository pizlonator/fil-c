# Negative for the narrowed dropRestore crossing exemption: the restore
# `movq 72(%r12), %rbx` IS a proven save/restore pair (slot 8 holds the pushed
# %rbx, so it is marked x86_dropRestore), but its carrier %r12 was parked
# AFTER the first mid-function `and` (between and#1 and and#2). The parked
# value carries and#1's dynamic slack, so the restore crosses and#2's slack
# and must still be rejected — only ENTRY-derived (pre-first-and, depth-0)
# carriers are exempt.
	.text
	.globl	crossbad
	.type	crossbad, @function
crossbad:                        ;! long(long,long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp
	jmp	.Lb1
.Lb1:
	and	$-32, %rsp
	movq	%rsp, %r12
	movq	%rdi, 0(%r12)
	jmp	.Lb2
.Lb2:
	and	$-32, %rsp
	movq	72(%r12), %rbx
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	crossbad, .-crossbad
	.section	.note.GNU-stack,"",@progbits
