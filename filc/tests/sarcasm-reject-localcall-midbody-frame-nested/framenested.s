# A frame-addressing mid-body label of a signatured function called from
# INSIDE another local subroutine's clone range (helper is a top-level
# call/ret subroutine; drive calls it). Even when the chain would end in the
# owner's own activation, the model compensates ONE return-address push per
# clone — not per nesting level — so the clone's rsp-relative accesses would
# key one push off; and if any other function reached the same subroutine the
# accesses would re-key to a foreign frame. Discovery rejects any call edge
# into a frame-addressing mid-body clone that is not a direct call from the
# owner's body.
	.text
	.globl	drive
	.type	drive, @function
drive:                          ;! long(long,long)
	movq	%rdi, %r10
	movq	%rsi, %r11
	call	helper
	movq	%r9, %rax
	ret
	.size	drive, .-drive
	.type	helper, @function
helper:
	call	.Lmid_touch         # nested call into a framey mid-body clone
	ret
	.size	helper, .-helper
	.globl	owner_fn
	.type	owner_fn, @function
owner_fn:                       ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	call	.Lmid_touch         # the owner's own (sound) direct self-call
	addq	$16, %rsp
	popq	%rbp
	movq	-8(%rbp), %rax
	ret
	.align	16
.Lmid_touch:
	addq	$100, -8(%rbp)      # frame access: the owner's -8(%rbp)
	movq	-16(%rbp), %r9
	movq	%r9, 8(%rsp)        # frame access: the owner's 0(%rsp)
	ret
	.size	owner_fn, .-owner_fn
	.section	.note.GNU-stack,"",@progbits