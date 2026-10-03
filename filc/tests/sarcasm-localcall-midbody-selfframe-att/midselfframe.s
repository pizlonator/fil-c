# A signatured function calling its OWN mid-body label where the cloned range
# addresses the owner's stack frame — sound only because the caller IS the
# owner (the clone runs on the owner's stack; the caller's frame IS the
# owner's frame):
# - rbp-relative accesses in the clone key the same as the owner's own (a
#   local call does not move rbp — no bias);
# - rsp-relative accesses key 8 lower (the +8 rule: a hardware `call` pushes
#   the return address, so the clone's `8(%rsp)` is the owner's `0(%rsp)` at
#   the callsite depth).
# The owner writes 11 to its 0(%rsp) slot (rbp-32; the 32-byte alloca keeps
# it distinct from the -8/-16(%rbp) slots); the clone rewrites that same slot
# through its 8(%rsp) spelling with 77; the owner reads it back as 77. If the
# clone's slot were keyed anywhere else, the read would return 11 and the
# test would fail. All callsites sit at one rsp depth (the localRet depth
# gate requires every clone ret at its clone entry's depth).
	.text
	.globl	midself
	.type	midself, @function
midself:                        ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	$11, 0(%rsp)        # the owner's slot at rbp-32 (its 0(%rsp))
	call	.Lmid_touch
	movq	-8(%rbp), %rax
	addq	-16(%rbp), %rax
	addq	0(%rsp), %rax       # the slot the clone wrote via its 8(%rsp)
	addq	$32, %rsp
	popq	%rbp
	ret
	.align	16
.Lmid_touch:
	addq	$100, -8(%rbp)      # rbp-relative: the owner's -8(%rbp), no bias
	movq	-16(%rbp), %r9
	imulq	$3, %r9
	movq	%r9, -16(%rbp)
	movq	$77, 8(%rsp)        # +8 rule: the owner's 0(%rsp) slot
	ret
	.size	midself, .-midself
	.section	.note.GNU-stack,"",@progbits