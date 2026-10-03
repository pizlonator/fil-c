	.file	"b2clone-short.s"
	.text
# REJECT: a `#! stack buffer (id)` SHORT form on a statement inside a B2
# shared-tail clone.
#
# fnC jumps into the middle of fnD, so fnD's reachable region is B2-cloned
# into fnC. The short form names a FILE-WIDE canonical range — a property of
# the declaring function's own frame — and a clone's window is private to the
# clone's own frame context, so a short form inside a cloned region has no
# meaningful resolution and is rejected fail-closed (the long form in fnD's
# own body is fine; the clone's copy of the SHORT-form statement is what
# fails). fnD compiled alone would be accepted.
#
# sarcasm: `stack buffer (kd)` short form on a shared-tail clone statement is
# not supported
	.globl	fnC
	.type	fnC, @function
fnC:                            ;! long(size_t)
	jmp	.LfnD_body
	.size	fnC, .-fnC

	.globl	fnD
	.type	fnD, @function
fnD:                            ;! long(size_t)
.LfnD_entry:
.LfnD_body:
	subq	$64, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
.LfnD_tail:
	movl	(%rsp,%rdi), %eax #! stack buffer (kd, %rsp, %rsp + 64)
	movl	4(%rsp), %ecx #! stack buffer (kd)
	addl	%ecx, %eax
	addq	$64, %rsp
	ret
	.size	fnD, .-fnD
	.section	.note.GNU-stack,"",@progbits
