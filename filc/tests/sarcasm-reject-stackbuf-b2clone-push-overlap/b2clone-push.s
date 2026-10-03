	.file	"b2clone-push.s"
	.text
# REJECT: a stack buffer overlapping a pushed register's save slot inside a
# B2 shared-tail clone.
#
# fnE jumps into the middle of fnF, cloning fnF's region (push, sub, tail,
# teardown, ret) into fnE. The clone's pushed %rbx parks a save slot at
# normalized [-8, 0) of the augmented body while the push is outstanding —
# below the jumper's frame, where the transient prologue push-pad model
# cannot see it — and the clone's buffer declaration [%rsp, %rsp + 72)
# resolves to [-72, 0), whose top 8 bytes ARE that slot. The slot's bytes
# are the pushed register's web (the matching pop is dropped and the
# save-slot model carries the value), so redirecting them into a real buffer
# region would give those bytes two homes. Rejected fail-closed for both the
# clone and fnF's own body (the same geometric rule applies there: fnF's own
# declaration resolves to [0, 72), overlapping its own push slot [64, 72)).
#
# sarcasm: stack buffer (kf) overlaps a pushed register's save slot at
# normalized [-8, 0)
	.globl	fnE
	.type	fnE, @function
fnE:                            ;! long(size_t)
	jmp	.LfnF_body
	.size	fnE, .-fnE

	.globl	fnF
	.type	fnF, @function
fnF:                            ;! long(size_t)
.LfnF_entry:
.LfnF_body:
	pushq	%rbx
	subq	$64, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
.LfnF_tail:
	movl	(%rsp,%rdi), %eax #! stack buffer (kf, %rsp, %rsp + 72)
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	fnF, .-fnF
	.section	.note.GNU-stack,"",@progbits
