# Caller-saved clone carrier across a real call (fail-closed): the clone
# parks the jump-site rsp in %rax (legal only by the B2-clone relaxation),
# but the annotated call clobbers every caller-saved register (SysV), so the
# parked value is gone on return. The frame pass poisons live caller-saved
# register carriers across non-local calls (proving the relaxation
# live-across-no-calls), so recovering %rsp through %rax afterwards revives
# no provable depth and the clone's `ret` is statically rejected. Without
# the call the same recovery compiles (the carrier is intact), and
# callee-saved carriers (preserved by the callee) and call-free spans are
# unaffected.
	.text
	.globl	ccall_jump
	.type	ccall_jump, @function
ccall_jump:                     ;! long(long)
	movq	%rdi, %rbx
	jmp	.Lccall_tail
	.size	ccall_jump, .-ccall_jump
	.globl	ccall_owner
	.type	ccall_owner, @function
ccall_owner:                    ;! long(long)
	movq	%rdi, %rbx
	nop
.Lccall_tail:
	# clone: park in caller-saved %rax, call across it, recover through it.
	movq	%rsp, %rax
	movq	%rbx, %rdi
	call	ccall_helper      ;! long(long)
	movq	%rax, %rsp
	movq	%rbx, %rax
	ret
	.size	ccall_owner, .-ccall_owner
	.globl	ccall_helper
	.type	ccall_helper, @function
ccall_helper:                   ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	ret
	.size	ccall_helper, .-ccall_helper
	.section	.note.GNU-stack,"",@progbits
