	.text
# Feature B: a caller-declared buffer used by the CLONE through the by-address
# mode (the rsaz MUL shape): the subroutine's own text walks a pointer seeded
# from its `leaq 8(%rsp)` (the +8 return-address compensation — hardware call
# semantics), and every store through it is annotated short-form and lowered to
# a runtime-checked raw access into the CALLER's lowered buffer. Two callers
# declare the SAME buffer name at the SAME range; each caller's clone keys the
# buffer through its own declaration.
	.globl	tc_a
	.type	tc_a, @function
tc_a:                           ;! long(long)
	pushq	%rbx
	subq	$32, %rsp
	movq	$0x11111111, 0(%rsp) #! stack buffer (b, %rsp, %rsp + 32)
	movl	%edi, %r10d
	call	bump1
	movq	0(%rsp), %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	tc_a, .-tc_a
	.globl	tc_b
	.type	tc_b, @function
tc_b:                           ;! long(long)
	pushq	%rbx
	subq	$32, %rsp
	movq	$0x22222222, 0(%rsp) #! stack buffer (b, %rsp, %rsp + 32)
	movl	%edi, %r10d
	call	bump1
	movq	0(%rsp), %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	tc_b, .-tc_b
	.type	bump1, @function
bump1:
	# the caller's buffer byte 0 through a walking pointer: the +8 spelling
	# names the caller's 0(%rsp) at the callsite (the clone model compensates)
	leaq	8(%rsp), %r9
	movl	(%r9), %r10d #! stack buffer (b)
	addl	$0x1000000, %r10d
	movl	%r10d, (%r9) #! stack buffer (b)
	ret
	.size	bump1, .-bump1
	.section	.note.GNU-stack,"",@progbits
