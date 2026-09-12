# Caller-saved clone carrier across a local call that clobbers it
# (fail-closed): a local call is no hardware call, so the frame pass does
# not poison caller-saved carriers across it — but the per-caller clone is
# inlined with the caller's carrier map, and this subroutine overwrites %rax
# outright. The recovery then revives no provable depth ("dyn"), and the
# clone's `ret` is statically rejected. A local subroutine that leaves the
# carrier alone keeps compiling (ordinary def-marking, no blanket poison).
	.text
	.globl	cloc_jump
	.type	cloc_jump, @function
cloc_jump:                      ;! long(long)
	movq	%rdi, %rbx
	jmp	.Lcloc_tail
	.size	cloc_jump, .-cloc_jump
	.globl	cloc_owner
	.type	cloc_owner, @function
cloc_owner:                      ;! long(long)
	movq	%rdi, %rbx
	nop
.Lcloc_tail:
	movq	%rsp, %rax
	call	cloc_sub
	movq	%rax, %rsp
	movq	%rbx, %rax
	ret
	.size	cloc_owner, .-cloc_owner
	.type	cloc_sub, @function
cloc_sub:
	movq	$5, %rax
	ret
	.size	cloc_sub, .-cloc_sub
	.section	.note.GNU-stack,"",@progbits
