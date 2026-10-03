	.file	"b2clone.s"
	.text
# `#! stack buffer (...)` on statements inside B2 shared-tail clones.
#
# fnA and fnA2 jump into the MIDDLE of fnB (an unannotated jump to a mid-body
# label of another function is a B2 shared-tail join: tailcall.luau clones
# fnB's reachable region into each jumper). The clone's `#! stack buffer`
# long form resolves IN THE CLONE'S OWN FRAME CONTEXT: the clone runs at the
# jumper's rsp and its own `sub` is ordinary code of the augmented body, so
# the same declaration that resolves to [0, 64) in fnB resolves to
# [-64, 0) inside each jumper — the same real bytes, two coordinate systems.
# Clone declarations do not participate in the file-wide canonical table and
# cannot satisfy short forms. Each clone's buffer is lowered into a region of
# the JUMPER's synthesized output frame, bounds-checked, and independent of
# every other clone's.

# First dispatcher: an unconditional tail jump into fnB's body.
	.globl	fnA
	.type	fnA, @function
fnA:                            ;! long(size_t)
	jmp	.LfnB_body
	.size	fnA, .-fnA

# Second dispatcher: a conditional tail jump into the same region (its own
# clone, its own buffer region, its own bounds check).
	.globl	fnA2
	.type	fnA2, @function
fnA2:                           ;! long(size_t)
	testq	%rdi, %rdi
	jz	.La2_zero
	jmp	.LfnB_body
.La2_zero:
	xorl	%eax, %eax
	ret
	.size	fnA2, .-fnA2

# The tail's owner; also called directly (its own body must keep working).
	.globl	fnB
	.type	fnB, @function
fnB:                            ;! long(size_t)
.LfnB_entry:
.LfnB_body:
	subq	$64, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
	movl	$0x33333333, 8(%rsp)
	movl	$0x55555555, 60(%rsp)
.LfnB_tail:
	# The long-form declaration: the indexed keystream-style load declares
	# [%rsp, %rsp + 64) as a raw byte buffer.
	movl	(%rsp,%rdi), %eax #! stack buffer (ks, %rsp, %rsp + 64)
	movl	4(%rsp), %ecx
	addl	%ecx, %eax
	addq	$64, %rsp
	ret
	.size	fnB, .-fnB
	.section	.note.GNU-stack,"",@progbits
