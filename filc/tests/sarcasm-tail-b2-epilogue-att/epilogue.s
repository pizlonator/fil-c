# B2 join whose clone carries its owner's WHOLE epilogue (the depth-0
# hatch's legitimate case): the jumper keeps its deep frame (no reshape
# before jumping), so the clone runs the owner's reshape `add`, the shared
# tail, and the owner's full teardown (`add` + paired pops + `ret`), returning
# at the entry rsp from a clone-entry depth of 152. The ret is accepted only
# because it sits inside a verified epilogue teardown span (one constant
# `add` plus paired callee-saved pops leading straight to `ret`) — an
# unverified path to depth 0 stays rejected (see
# sarcasm-reject-b2-dyndepth-att). Executes through the join to prove the
# teardown-in-clone shape returns correctly (a miscompiled epilogue would
# corrupt the return address and crash).
	.text
	.globl	deep_jump
	.type	deep_jump, @function
deep_jump:                      ;! long(long,long)
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp              # depth 152
	movq	%rdi, %rbx
	addq	%rsi, %rbx              # rbx = a + b (shared register state)
	jmp	.Lmid_shared
	.size	deep_jump, .-deep_jump
	.globl	mid_frame
	.type	mid_frame, @function
mid_frame:                      ;! long(long,long)
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp              # depth 152
	movq	%rdi, %rbx
	addq	%rsi, %rbx              # rbx = a + b
	nop
.Lmid_shared:
	# shared region: reshape into the shallow working frame, run the tail
	# through frame slots, then carry the whole epilogue to `ret`.
	addq	$64, %rsp               # 152 -> 88 (mid-function reshape)
	movq	%rbx, 0(%rsp)
	movq	0(%rsp), %rax
	addq	%rbx, %rax
	addq	$5, %rax
	addq	$40, %rsp               # 88 -> 48 (verified teardown span below)
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	ret
	.size	mid_frame, .-mid_frame
	.section	.note.GNU-stack,"",@progbits
