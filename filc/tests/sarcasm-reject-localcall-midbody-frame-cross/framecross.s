# A cross-function call to a mid-body label whose clone range addresses the
# OWNER's stack frame. The clone would run in the other function's
# activation, where the frame pass would re-key its rsp/rbp-relative
# accesses against a frame the cloned text was not written against — a
# silent miscompile — so discovery rejects it. (The owner's own self-call of
# the same label is the sound case: see sarcasm-localcall-midbody-selfframe-att.)
	.text
	.globl	owner_fn
	.type	owner_fn, @function
owner_fn:                       ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	$0, 0(%rsp)
	call	.Lmid_touch
	movq	-8(%rbp), %rax
	addq	-16(%rbp), %rax
	addq	0(%rsp), %rax
	addq	$16, %rsp
	popq	%rbp
	ret
	.align	16
.Lmid_touch:
	addq	$100, -8(%rbp)      # frame access: the owner's -8(%rbp)
	movq	-16(%rbp), %r9
	movq	%r9, 8(%rsp)        # frame access: the owner's 0(%rsp)
	ret
	.size	owner_fn, .-owner_fn
	.globl	other_caller
	.type	other_caller, @function
other_caller:                   ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	call	.Lmid_touch         # cross-function: rejected at discovery
	movq	-8(%rbp), %rax
	addq	$16, %rsp
	popq	%rbp
	ret
	.size	other_caller, .-other_caller
	.section	.note.GNU-stack,"",@progbits