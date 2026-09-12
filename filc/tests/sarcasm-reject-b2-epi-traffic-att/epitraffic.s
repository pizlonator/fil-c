# Epilogue-to-zero with extra traffic inside the teardown (fail-closed):
# the jumper keeps its deep frame (depth 152) and the clone reshapes and
# carries the whole epilogue toward the entry rsp — but a frame-touching
# store sits between the last `add` and the `ret`. validateTeardown accepts
# only restoring operations (one constant adjustment, paired callee-saved
# pops, frame-touch-free computation) on a straight path to `ret`, so the
# extra store breaks every candidate span covering the ret: no x86_teardown
# mark, no depth-0 hatch, and 0 != 152 is an unbalanced clone frame.
# Static error at the clone's `ret`. Companion to
# sarcasm-tail-b2-epilogue-att (the same shape with a clean span, accepted):
# together they pin that the hatch proves restores (shape + pop identity +
# dRet == 0 completeness) rather than merely landing on zero.
	.text
	.globl	etraffic_jump
	.type	etraffic_jump, @function
etraffic_jump:                  ;! long(long,long)
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp              # depth 152
	movq	%rdi, %rbx
	addq	%rsi, %rbx
	jmp	.Letraffic_shared
	.size	etraffic_jump, .-etraffic_jump
	.globl	etraffic_frame
	.type	etraffic_frame, @function
etraffic_frame:                 ;! long(long,long)
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp              # depth 152
	movq	%rdi, %rbx
	addq	%rsi, %rbx
	nop
.Letraffic_shared:
	addq	$64, %rsp               # 152 -> 88 (mid-function reshape)
	movq	%rbx, 0(%rsp)
	movq	0(%rsp), %rax
	addq	%rbx, %rax
	addq	$40, %rsp               # 88 -> 48
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	movq	$0, 0(%rsp)             # extra frame-touching traffic: no span
	ret
	.size	etraffic_frame, .-etraffic_frame
	.section	.note.GNU-stack,"",@progbits
