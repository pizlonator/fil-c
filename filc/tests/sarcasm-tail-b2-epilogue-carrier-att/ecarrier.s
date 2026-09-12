# B2 join whose clone carries a CARRIER-RECOVERY epilogue (the early-run
# skip's load-bearing case): both functions park the header rsp in
# caller-saved %rax — a save the region-less early geometry run cannot
# record (its region corner needs the regions, and the header is not a
# clone) — and the shared tail recovers %rsp through it, redefines %rax,
# and tears the frame down to `ret`.
#
# Checking balance on the early run would reject this shape (the recovery
# revives nothing there, so the clone's `ret` sits at "dyn"), while the
# region-carrying runs record the save and prove it: recovery to the
# clone-entry depth, then a verified teardown span (constant `add` + paired
# pops) to the entry rsp, accepted by the depth-0 hatch. Executes through
# the join to prove the recovered rsp addresses the teardown correctly.
# The region-carrying balance proof (runs 2 and 3) plus the rewrite's
# cloneRetProofRan machine check back the exemption; the early run's marks
# are discarded (analyzeFrame clears every x86_* mark at entry) and its
# frameSize superseded (the transform adopts the region-carrying run's).
	.text
	.globl	ecar_jump
	.type	ecar_jump, @function
ecar_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsp, %rax
	jmp	.Lecar_tail
	.size	ecar_jump, .-ecar_jump
	.globl	ecar_owner
	.type	ecar_owner, @function
ecar_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsp, %rax
	nop
.Lecar_tail:
	movq	%rax, %rsp
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	ecar_owner, .-ecar_owner
	.section	.note.GNU-stack,"",@progbits
