# A loop that can break BEFORE reaching the ud2 (the `je .Lend` break edge is
# live — it is tested before the trap), with no `ret` after the loop: the break
# path reaches .Lend and falls off the end of the body, so the body is
# rejected. Only paths THROUGH the ud2 are terminated by it.
	.text
	.globl	ud2_break_before
	.type	ud2_break_before, @function
ud2_break_before:               ;! void(long)
	endbr64
.Ltop:
	testq	%rdi, %rdi
	je	.Lend
	ud2
	jmp	.Ltop
.Lend:
	.size	ud2_break_before, .-ud2_break_before
	.section	.note.GNU-stack,"",@progbits
